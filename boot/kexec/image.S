/*
 * Copyright (C) 2026 miniosv contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

/*
 * The miniosv kexec image for aarch64: an arm64 Linux "Image" that a running
 * Linux can kexec into (kexec_file_load() or kexec-tools' kexec_load()).
 *
 * Layout of loader.kexec, which Linux copies as one block to a physical
 * address P = B + text_offset for some 2 MiB-aligned B it picks, zero-filling
 * up to image_size, and enters at P with the MMU and caches off, at EL1, with
 * x0 = physical address of the device tree it hands over:
 *
 *   P + 0x00000   64-byte Image header (Documentation/arch/arm64/booting.rst)
 *   P + 0x01000   boot_info the trampoline fills (osv/boot-info.hh)
 *   P + 0x10000   the kernel ELF (loader-stripped.elf, KERNEL_ELF_FILE)
 *
 * The kernel ELF is a single PT_LOAD at file offset 0 linked at
 * KERNEL_ELF_VADDR, so the file is already its memory image: with
 * text_offset = 0x90000 - 0x10000 the ELF header lands at B + 0x90000 and
 * text_start at B + 0xa0000, exactly the geometry arch/aarch64/boot.S
 * (calculate_virt_offset, fixup_boot_pt) assumes for a kernel the UEFI stub
 * loaded - no copy is needed. image_size (KERNEL_IMAGE_SIZE, computed by the
 * Makefile from .edata) covers the ELF's .bss so Linux keeps that memory free
 * and zeroed; it must be exact, as kexec-tools places the DTB right above.
 *
 * The trampoline writes the kexec boot_info (magic, kernel_phys_base and the
 * DTB address; the kernel derives the rest from the DTB and the UEFI tables
 * it points at, see arch/aarch64/kexec-boot.cc) and jumps to the ELF entry,
 * uefi_entry, which reads kernel_phys_base from boot_info and continues as
 * after a UEFI boot. Everything here is PC-relative: Linux applies no
 * relocations and the load address is only known at run time.
 */

#ifndef KERNEL_ELF_FILE
#error "KERNEL_ELF_FILE must be defined to the path of the kernel ELF"
#endif
#ifndef KERNEL_ELF_VADDR
#error "KERNEL_ELF_VADDR must be the link address of the kernel ELF header"
#endif
#ifndef KERNEL_IMAGE_SIZE
#error "KERNEL_IMAGE_SIZE must be the Image size including the kernel's .bss"
#endif

#define KEXEC_BOOT_INFO_OFFSET  0x1000
#define KEXEC_ELF_OFFSET        0x10000
/* Puts the ELF header at B + 0x90000 (see above). */
#define KEXEC_TEXT_OFFSET       (0x90000 - KEXEC_ELF_OFFSET)

/* Image header flags: little-endian, 4 KiB pages, placed anywhere in RAM. */
#define IMAGE_FLAGS             ((1 << 1) | (1 << 3))

/* osv::boot_info_magic_kexec ("OSVKBIF1") and field offsets (osv/boot-info.hh). */
#define BOOT_INFO_MAGIC_KEXEC   0x4f53564b42494631
#define BOOT_INFO_KERNEL_PHYS   40
#define BOOT_INFO_DTB_ADDR      72

        .section .head, "ax"
        .globl _head
_head:
        b       kexec_start                     /* code0 */
        .long   0                               /* code1 */
        .quad   KEXEC_TEXT_OFFSET               /* text_offset */
        .quad   KERNEL_IMAGE_SIZE               /* image_size */
        .quad   IMAGE_FLAGS                     /* flags */
        .quad   0, 0, 0                         /* res2, res3, res4 */
        .ascii  "ARM\x64"                       /* magic */
        .long   0                               /* res5 (PE header offset) */

kexec_start:
        /* x0 = DTB physical address (Linux), x1-x3 = 0. */
        adr     x20, _head
        add     x21, x20, #KEXEC_ELF_OFFSET     /* kernel_phys_base: the ELF header */
        adr     x22, kexec_boot_info

        ldr     x1, =BOOT_INFO_MAGIC_KEXEC
        str     x1, [x22]
        str     x21, [x22, #BOOT_INFO_KERNEL_PHYS]
        str     x0, [x22, #BOOT_INFO_DTB_ADDR]

        /* Entry point: e_entry (ELF header offset 24) relocated from the link
           address to where the ELF sits now. */
        ldr     x1, [x21, #24]
        ldr     x2, =KERNEL_ELF_VADDR
        sub     x1, x1, x2
        add     x1, x1, x21

        mov     x0, x22
        br      x1
        .ltorg

        .org    KEXEC_BOOT_INFO_OFFSET
kexec_boot_info:
        .fill   4096, 1, 0

        .org    KEXEC_ELF_OFFSET
kernel_elf_start:
        .incbin KERNEL_ELF_FILE
