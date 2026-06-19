#pragma once

// MiniDPDK shim for DPDK's <rte_memzone.h>. A memzone is a named region of
// physically-contiguous, DMA-able memory. Types and free functions at global
// scope, mirroring DPDK; the allocation is backed by OSv's
// memory::alloc_phys_contiguous_aligned + mmu::virt_to_phys.
//
// Only the subset the ENA driver references is provided.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include <osv/contiguous_alloc.hh>
#include <osv/virt_to_phys.hh>

#include <minidpdk/rte_memory.h>

#define RTE_MEMZONE_NAMESIZE 32

// memzone flag. All OSv phys-contiguous allocations are already IOVA-contiguous,
// so this is accepted and ignored. Value matches DPDK.
#define RTE_MEMZONE_IOVA_CONTIG 0x00100000

// A memzone descriptor. Only the fields the ENA driver reads (addr, iova) plus
// the standard name/len are modelled.
struct rte_memzone {
    char name[RTE_MEMZONE_NAMESIZE];
    uint64_t iova;   // physical (IO) address of the region
    void *addr;      // virtual address of the region
    size_t len;      // length of the region in bytes
};

// Reserve `len` bytes of physically-contiguous, DMA-able memory aligned to
// `align`. socket_id and flags are accepted for DPDK compatibility but ignored:
// the unikernel has a single NUMA domain and every region is IOVA-contiguous.
// The descriptor is heap-owned and stays valid until rte_memzone_free(); the
// IOVA is just the region's physical address (OSv has no IOMMU remapping here).
// Returns nullptr on allocation failure.
inline const rte_memzone *
rte_memzone_reserve_aligned(const char *name, size_t len, int socket_id,
                            unsigned flags, unsigned align) {
    (void)socket_id;
    (void)flags;

    // OSv asserts a power-of-two alignment; DPDK bumps sub-cache-line requests
    // up to a cache line, so honour at least that (ENA passes 64).
    if (align < 64) {
        align = 64;
    }

    void *va = memory::alloc_phys_contiguous_aligned(len, align);
    if (!va) {
        return nullptr;
    }

    auto *mz = new (std::nothrow) rte_memzone;
    if (!mz) {
        memory::free_phys_contiguous_aligned(va);
        return nullptr;
    }

    std::strncpy(mz->name, name ? name : "", sizeof(mz->name) - 1);
    mz->name[sizeof(mz->name) - 1] = '\0';
    mz->addr = va;
    mz->iova = mmu::virt_to_phys(va);
    mz->len = len;
    return mz;
}

inline int rte_memzone_free(const rte_memzone *mz) {
    if (!mz) {
        return -1;
    }
    memory::free_phys_contiguous_aligned(mz->addr);
    delete mz;
    return 0;
}
