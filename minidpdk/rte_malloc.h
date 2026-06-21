#pragma once

// MiniDPDK shim for DPDK's <rte_malloc.h>. Plain (non-DMA) allocations routed
// through OSv's libc allocator: posix_memalign for the aligned, zero-filled
// blocks DPDK's zmalloc returns, and free to release them. Socket/NUMA arguments
// are ignored (single NUMA domain). Only the subset the ENA driver references is
// provided.

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <stdlib.h>

#include <minidpdk/rte_memory.h>

// Zero-filled allocation of `size` bytes aligned to `align`. `type` is a DPDK
// debug tag and `socket` a NUMA hint; both are ignored. `align` of 0 means no
// constraint — clamp up to the pointer size so posix_memalign always gets a
// valid (power-of-two, >= sizeof(void*)) alignment. Returns nullptr on failure.
inline void *rte_zmalloc_socket(const char *type, size_t size, unsigned align,
                                int socket) {
    (void)type;
    (void)socket;
    size_t a = align > sizeof(void *) ? align : sizeof(void *);
    void *p = nullptr;
    if(!(p = aligned_alloc(a, size)))
        return nullptr;
    std::memset(p, 0, size);
    return p;
}

inline void *rte_zmalloc(const char *type, size_t size, unsigned align) {
    return rte_zmalloc_socket(type, size, align, SOCKET_ID_ANY);
}

inline void rte_free(void *ptr) {
    free(ptr);
}
