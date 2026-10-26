/*
 * Copyright (C) 2026 miniosv contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef OSV_BOOT_ACPI_HH
#define OSV_BOOT_ACPI_HH

#include <stdint.h>

//
// Minimal ACPI table lookups shared by the boot stubs (the UEFI stub in
// boot/uefi/, the kexec entry in arch/aarch64/kexec-boot.cc) and usable
// before the kernel's ACPI layer exists: they walk the raw tables through
// whatever physical mapping the caller runs under. Header-only and
// freestanding: no libc beyond __builtin_memcpy.
//

namespace osv {
namespace boot_acpi {

// Unaligned little-endian reads (ACPI table fields are not naturally aligned).
inline uint32_t rd32(const void *p)
{
    uint32_t v;
    __builtin_memcpy(&v, p, 4);
    return v;
}

inline uint64_t rd64(const void *p)
{
    uint64_t v;
    __builtin_memcpy(&v, p, 8);
    return v;
}

// The SPCR (Serial Port Console Redirection) fields the kernel needs to pick
// and configure the console driver. Interface Type (offset 36) tells
// PL011/SBSA (3/0x0e) apart from 16550 (0/1); the GAS Access Size (offset 43)
// gives the register access width (1=byte, 2=word, 3=dword) - AWS Graviton's
// 16550 is dword (mmio32).
struct spcr_uart {
    uint64_t base;          // physical base address, 0 if no SPCR
    uint8_t  iface_type;    // 0xff if no SPCR
    uint8_t  access_size;   // GAS AccessSize: 0=undef,1=8b,2=16b,3=32b,4=64b
};

// Find the console UART described by the SPCR table under the given RSDP.
inline spcr_uart find_spcr_uart(uint64_t rsdp)
{
    spcr_uart out = { 0, 0xff, 0 };
    if (!rsdp)
        return out;
    auto r = reinterpret_cast<const unsigned char *>(rsdp);
    // RSDP: XsdtAddress at offset 24 (ACPI 2.0+), RsdtAddress at offset 16.
    uint64_t xsdt = rd64(r + 24);
    uint64_t rsdt = rd32(r + 16);
    uint64_t hdr = xsdt ? xsdt : rsdt;
    int entsize = xsdt ? 8 : 4;
    if (!hdr)
        return out;
    auto h = reinterpret_cast<const unsigned char *>(hdr);
    uint32_t len = rd32(h + 4);
    uint32_t n = (len > 36) ? (len - 36) / entsize : 0;
    for (uint32_t i = 0; i < n; i++) {
        uint64_t tbl = (entsize == 8) ? rd64(h + 36 + i * 8)
                                      : (uint64_t)rd32(h + 36 + i * 4);
        auto t = reinterpret_cast<const unsigned char *>(tbl);
        if (t[0] == 'S' && t[1] == 'P' && t[2] == 'C' && t[3] == 'R') {
            out.iface_type = t[36];   // Interface Type
            out.access_size = t[43];  // GAS AccessSize
            out.base = rd64(t + 44);  // Base Address (GAS Address field)
            return out;
        }
    }
    return out;
}

// GAS AccessSize -> register access width in bytes (1/2/4); byte when the
// firmware leaves it undefined.
inline uint8_t spcr_access_width(uint8_t access_size)
{
    return access_size == 3 ? 4 : access_size == 2 ? 2 : 1;
}

} // namespace boot_acpi
} // namespace osv

#endif /* OSV_BOOT_ACPI_HH */
