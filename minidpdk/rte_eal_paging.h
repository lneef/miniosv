#pragma once

// MiniDPDK shim for DPDK's <rte_eal_paging.h>. Only rte_mem_page_size() is
// provided -- the single symbol the ENA driver references. The page size comes
// from OSv's mmu (mmu::page_size) instead of an OS syscall.

#include <cstddef>

#include <osv/mmu.hh>

// System page size in bytes. Never fails. Backed by OSv's mmu page size.
inline constexpr size_t rte_mem_page_size() {
    return mmu::page_size;
}
