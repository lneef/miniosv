#pragma once

// MiniDPDK shim for DPDK's <rte_prefetch.h>. OSv has no prefetch helper, so we
// use the compiler builtin __builtin_prefetch (no inline asm, arch-portable).
// Only the subset the ENA driver references is provided.
//
// __builtin_prefetch(addr, rw, locality): rw 0=read 1=write; locality 3 keeps
// the line in all cache levels, matching DPDK's rte_prefetch0 semantics.

inline void rte_prefetch0(const volatile void *p) {
    __builtin_prefetch((const void *)p, 0, 3);
}
inline void rte_prefetch0_write(const volatile void *p) {
    __builtin_prefetch((const void *)p, 1, 3);
}
