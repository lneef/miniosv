/*
 * Copyright (C) 2026 miniosv contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

//
// Boot information for a kernel started by Linux kexec.
//
// Linux enters the kexec image (boot/kexec/image.S) with x0 pointing at a
// flattened device tree. Linux itself was booted by UEFI, so that tree is the
// one its EFI stub made: no /memory nodes, but /chosen carries the UEFI
// memory map and system table it preserved (linux,uefi-mmap-*,
// linux,uefi-system-table - drivers/firmware/efi/libstub/fdt.c), plus the
// bootargs, an optional initrd and memory reservations. kexec_file_load()
// copies that tree and adds its own properties (drivers/of/kexec.c);
// kexec-tools edits /sys/firmware/fdt the same way.
//
// From that we rebuild what the UEFI stub would have put in boot_info:
//   - the ACPI RSDP, from the EFI system table's configuration tables,
//   - the memory map, from the preserved UEFI map, classified like the stub
//     does, minus what Linux placed in RAM without telling the UEFI map: the
//     DTB, the /memreserve/ entries, the initrd, and the EFI memreserve
//     chain (where the GICv3 LPI tables Linux left live),
//   - the console UART, from the ACPI SPCR,
//   - the command line, from /chosen/bootargs.
// No wall clock: UEFI runtime services cannot be called.
//
// This runs with the boot page tables (identity map of the first 512 GiB)
// before the console exists, so it only records an error for
// arch_init_early_console() to report.
//

#include <osv/types.h>
#include <osv/boot-info.hh>
#include <osv/boot-acpi.hh>
#include <string.h>

#include "kexec-boot.hh"

extern osv::boot_info *osv_boot_info;

namespace {

const char *boot_error;

void fail(const char *msg)
{
    if (!boot_error)
        boot_error = msg;
}

// Kernel-side copies of the hand-off arrays. They live in .bss, so they are
// reachable through the kernel's own mapping for as long as the kernel runs
// (the stub's low-RAM copies are only guaranteed under the boot tables).
constexpr u32 MAX_MEMMAP = 512;
constexpr u32 CMDLINE_CAP = 4096;
osv::boot_memmap_entry memmap[MAX_MEMMAP];
u32 memmap_count;
char cmdline[CMDLINE_CAP];

// ---- flattened device tree (big-endian) -------------------------------------

inline u32 be32(const void *p)
{
    auto b = static_cast<const u8 *>(p);
    return (u32)b[0] << 24 | (u32)b[1] << 16 | (u32)b[2] << 8 | b[3];
}

inline u64 be64(const void *p)
{
    return (u64)be32(p) << 32 | be32(static_cast<const u8 *>(p) + 4);
}

constexpr u32 FDT_MAGIC = 0xd00dfeed;
constexpr u32 FDT_BEGIN_NODE = 1;
constexpr u32 FDT_END_NODE = 2;
constexpr u32 FDT_PROP = 3;
constexpr u32 FDT_NOP = 4;
constexpr u32 FDT_END = 9;

// What we take from /chosen.
struct chosen_props {
    const char *bootargs = nullptr;
    u32 bootargs_len = 0;
    u64 sys_table = 0;
    u64 mmap_start = 0;
    u32 mmap_size = 0, mmap_desc_size = 0, mmap_desc_ver = 0;
    u64 initrd_start = 0, initrd_end = 0;
    const u8 *usable_range = nullptr;   // linux,usable-memory-range (crash kernels)
    u32 usable_range_len = 0;
};

// Address-or-size property: u32 or u64 depending on the writer.
u64 fdt_cell_value(const u8 *val, u32 len)
{
    return len == 8 ? be64(val) : len == 4 ? be32(val) : 0;
}

void chosen_prop(chosen_props &c, const char *name, const u8 *val, u32 len)
{
    if (!strcmp(name, "bootargs")) {
        c.bootargs = reinterpret_cast<const char *>(val);
        c.bootargs_len = len;
    } else if (!strcmp(name, "linux,uefi-system-table")) {
        c.sys_table = fdt_cell_value(val, len);
    } else if (!strcmp(name, "linux,uefi-mmap-start")) {
        c.mmap_start = fdt_cell_value(val, len);
    } else if (!strcmp(name, "linux,uefi-mmap-size")) {
        c.mmap_size = fdt_cell_value(val, len);
    } else if (!strcmp(name, "linux,uefi-mmap-desc-size")) {
        c.mmap_desc_size = fdt_cell_value(val, len);
    } else if (!strcmp(name, "linux,uefi-mmap-desc-ver")) {
        c.mmap_desc_ver = fdt_cell_value(val, len);
    } else if (!strcmp(name, "linux,initrd-start")) {
        c.initrd_start = fdt_cell_value(val, len);
    } else if (!strcmp(name, "linux,initrd-end")) {
        c.initrd_end = fdt_cell_value(val, len);
    } else if (!strcmp(name, "linux,usable-memory-range")) {
        c.usable_range = val;
        c.usable_range_len = len;
    }
}

// Walk the structure block and collect the /chosen properties. Returns false
// on a malformed tree.
bool fdt_read_chosen(const u8 *fdt, chosen_props &c)
{
    u32 off_struct = be32(fdt + 8), off_strings = be32(fdt + 12);
    u32 size_struct = be32(fdt + 36);
    const u8 *p = fdt + off_struct, *end = p + size_struct;
    const char *strings = reinterpret_cast<const char *>(fdt + off_strings);
    int depth = 0;
    bool in_chosen = false;    // a node at depth 2 named "chosen" is open

    while (p + 4 <= end) {
        u32 tok = be32(p);
        p += 4;
        switch (tok) {
        case FDT_BEGIN_NODE: {
            auto name = reinterpret_cast<const char *>(p);
            p += (strlen(name) + 1 + 3) & ~3u;
            depth++;
            if (depth == 2)
                in_chosen = !strcmp(name, "chosen");
            break;
        }
        case FDT_END_NODE:
            if (depth == 2)
                in_chosen = false;
            depth--;
            break;
        case FDT_PROP: {
            u32 len = be32(p), nameoff = be32(p + 4);
            p += 8;
            if (in_chosen && depth == 2)
                chosen_prop(c, strings + nameoff, p, len);
            p += (len + 3) & ~3u;
            break;
        }
        case FDT_NOP:
            break;
        case FDT_END:
            return true;
        default:
            return false;
        }
    }
    return false;
}

// ---- memory map ----------------------------------------------------------------

// UEFI memory descriptor (EFI_MEMORY_DESCRIPTOR); entries are desc_size apart.
struct efi_memdesc {
    u32 type, pad;
    u64 phys, virt, pages, attr;
};
constexpr u32 EfiLoaderCode = 1;
constexpr u32 EfiBootServicesCode = 3;
constexpr u32 EfiBootServicesData = 4;
constexpr u32 EfiConventionalMemory = 7;
constexpr u64 EFI_MEMORY_RUNTIME = 1ull << 63;
constexpr u64 EFI_PAGE = 4096;

const u8 *efi_map;
u32 efi_map_size, efi_desc_size;

const efi_memdesc *efi_desc(u32 i)
{
    return reinterpret_cast<const efi_memdesc *>(efi_map + (u64)i * efi_desc_size);
}

u32 efi_desc_count()
{
    return efi_desc_size ? efi_map_size / efi_desc_size : 0;
}

// Linux may have called SetVirtualAddressMap(): pointers inside the runtime
// tables are then virtual. Map them back through the memory map (the same
// efi_to_phys as drivers/firmware/efi/efi-init.c). An address outside every
// runtime region is already physical.
u64 efi_to_phys(u64 addr)
{
    for (u32 i = 0; i < efi_desc_count(); i++) {
        auto d = efi_desc(i);
        if (!(d->attr & EFI_MEMORY_RUNTIME))
            continue;
        u64 size = d->pages * EFI_PAGE;
        if (addr >= d->virt && addr < d->virt + size)
            return d->phys + (addr - d->virt);
    }
    return addr;
}

void page_align_out(u64 &addr, u64 &size)
{
    u64 end = (addr + size + EFI_PAGE - 1) & ~(EFI_PAGE - 1);
    addr &= ~(EFI_PAGE - 1);
    size = end - addr;
}

// Take [addr, addr+size) out of the usable ranges, splitting one if needed.
void exclude(u64 addr, u64 size)
{
    if (!size)
        return;
    page_align_out(addr, size);
    u64 end = addr + size;
    u32 n = memmap_count;   // entries appended below need no further look
    for (u32 i = 0; i < n; i++) {
        auto &e = memmap[i];
        if (e.type != osv::boot_mem_type::usable || !e.size)
            continue;
        u64 es = e.addr, ee = e.addr + e.size;
        if (end <= es || addr >= ee)
            continue;
        if (addr <= es && end >= ee) {
            e.size = 0;
        } else if (addr > es && end < ee) {
            if (memmap_count >= MAX_MEMMAP) {
                fail("memory map too fragmented");
                return;
            }
            memmap[memmap_count++] = { end, ee - end, osv::boot_mem_type::usable };
            e.size = addr - es;
        } else if (addr <= es) {
            e.addr = end;
            e.size = ee - end;
        } else {
            e.size = addr - es;
        }
    }
}

// Keep only usable memory inside [addr, addr+size) (crash kernels get a
// linux,usable-memory-range and must stay out of the crashed kernel's RAM).
void restrict_usable(u64 addr, u64 size)
{
    exclude(0, addr);
    exclude(addr + size, ~0ull - (addr + size));
}

void drop_empty_entries()
{
    u32 j = 0;
    for (u32 i = 0; i < memmap_count; i++) {
        if (memmap[i].size)
            memmap[j++] = memmap[i];
    }
    memmap_count = j;
}

void build_memmap()
{
    memmap_count = 0;
    for (u32 i = 0; i < efi_desc_count() && memmap_count < MAX_MEMMAP; i++) {
        auto d = efi_desc(i);
        // Same classification as the UEFI stub: what Linux's own loader took
        // (EfiLoaderCode) is RAM to us, as are the boot-services regions.
        bool usable = d->type == EfiConventionalMemory ||
                      d->type == EfiBootServicesCode ||
                      d->type == EfiBootServicesData ||
                      d->type == EfiLoaderCode;
        memmap[memmap_count++] = { d->phys, d->pages * EFI_PAGE,
                                   usable ? osv::boot_mem_type::usable
                                          : osv::boot_mem_type::reserved };
    }
}

// ---- EFI system table ---------------------------------------------------------

struct efi_guid {
    u32 a;
    u16 b, c;
    u8 d[8];
};

bool guid_eq(const efi_guid &x, const efi_guid &y)
{
    return !memcmp(&x, &y, sizeof(efi_guid));
}

constexpr efi_guid ACPI_20_TABLE_GUID =
    { 0x8868e871, 0xe4f1, 0x11d3, { 0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81 } };
constexpr efi_guid ACPI_10_TABLE_GUID =
    { 0xeb9d2d30, 0x2d88, 0x11d3, { 0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d } };
// Linux's persistent memory reservations (include/linux/efi.h): a chain of
// pages, each {int size; int count; u64 next; {u64 base; u64 size;} entry[]}.
constexpr efi_guid LINUX_EFI_MEMRESERVE_TABLE_GUID =
    { 0x888eb0c6, 0x8ede, 0x4ff5, { 0xa8, 0xf0, 0x9a, 0xee, 0x5c, 0xb9, 0x77, 0xc2 } };

// EFI_SYSTEM_TABLE: NumberOfTableEntries at byte 104, ConfigurationTable at
// 112; each EFI_CONFIGURATION_TABLE is a 16-byte GUID and an 8-byte pointer.
u64 find_config_table(u64 sys_table, const efi_guid &guid)
{
    auto st = reinterpret_cast<const u8 *>(sys_table);
    u64 n = *reinterpret_cast<const u64 *>(st + 104);
    auto tab = reinterpret_cast<const u8 *>(
        efi_to_phys(*reinterpret_cast<const u64 *>(st + 112)));
    for (u64 i = 0; i < n; i++) {
        auto e = tab + i * 24;
        if (guid_eq(*reinterpret_cast<const efi_guid *>(e), guid))
            return *reinterpret_cast<const u64 *>(e + 16);
    }
    return 0;
}

void exclude_efi_memreserve(u64 sys_table)
{
    u64 node = find_config_table(sys_table, LINUX_EFI_MEMRESERVE_TABLE_GUID);
    for (int hops = 0; node && hops < 4096; hops++) {
        auto p = reinterpret_cast<const u8 *>(node);
        int size = *reinterpret_cast<const int *>(p);
        int count = *reinterpret_cast<const int *>(p + 4);
        u64 next = *reinterpret_cast<const u64 *>(p + 8);
        if (size < 0 || count < 0 || count > size)
            break;
        exclude(node, 16 + (u64)size * 16);
        for (int i = 0; i < count; i++) {
            auto ent = p + 16 + (u64)i * 16;
            exclude(*reinterpret_cast<const u64 *>(ent),
                    *reinterpret_cast<const u64 *>(ent + 8));
        }
        node = next;
    }
}

void prepare(osv::boot_info *bi)
{
    auto fdt = reinterpret_cast<const u8 *>(bi->dtb_addr);
    if (!fdt || be32(fdt) != FDT_MAGIC)
        return fail("no device tree from kexec");
    u32 totalsize = be32(fdt + 4);

    chosen_props c;
    if (!fdt_read_chosen(fdt, c))
        return fail("malformed device tree");
    if (!c.sys_table || !c.mmap_start || !c.mmap_size || !c.mmap_desc_size)
        return fail("device tree carries no UEFI memory map (Linux not booted by UEFI?)");
    if (c.mmap_desc_ver != 1)
        return fail("unsupported UEFI memory descriptor version");

    efi_map = reinterpret_cast<const u8 *>(c.mmap_start);
    efi_map_size = c.mmap_size;
    efi_desc_size = c.mmap_desc_size;
    build_memmap();

    // Memory Linux handed us that the UEFI map does not know about.
    exclude(bi->dtb_addr, totalsize);
    for (auto r = fdt + be32(fdt + 16); be64(r) || be64(r + 8); r += 16)
        exclude(be64(r), be64(r + 8));
    if (c.initrd_end > c.initrd_start)
        exclude(c.initrd_start, c.initrd_end - c.initrd_start);
    u64 sys_table = efi_to_phys(c.sys_table);
    exclude_efi_memreserve(sys_table);
    if (c.usable_range && c.usable_range_len >= 16)
        restrict_usable(be64(c.usable_range), be64(c.usable_range + 8));
    drop_empty_entries();
    if (boot_error)
        return;

    u64 rsdp = efi_to_phys(find_config_table(sys_table, ACPI_20_TABLE_GUID));
    if (!rsdp)
        rsdp = efi_to_phys(find_config_table(sys_table, ACPI_10_TABLE_GUID));
    if (!rsdp)
        return fail("no ACPI RSDP in the UEFI configuration tables");
    auto uart = osv::boot_acpi::find_spcr_uart(rsdp);

    u32 len = c.bootargs_len;
    if (len && !c.bootargs[len - 1])
        len--;
    if (len >= CMDLINE_CAP)
        len = CMDLINE_CAP - 1;
    memcpy(cmdline, c.bootargs, len);
    cmdline[len] = 0;

    bi->acpi_rsdp = rsdp;
    bi->memmap_addr = reinterpret_cast<u64>(memmap);
    bi->memmap_count = memmap_count;
    bi->cmdline_addr = reinterpret_cast<u64>(cmdline);
    bi->cmdline_len = len;
    bi->boot_unixtime_ns = 0;
    bi->uart_base = uart.base;
    bi->uart_type = uart.iface_type;
    bi->uart_access_width = osv::boot_acpi::spcr_access_width(uart.access_size);
}

} // namespace

void kexec_boot_prepare()
{
    auto bi = osv_boot_info;
    if (!bi || bi->magic != osv::boot_info_magic_kexec)
        return;
    prepare(bi);
}

const char *kexec_boot_error()
{
    return boot_error;
}
