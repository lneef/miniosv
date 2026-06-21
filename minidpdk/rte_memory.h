#pragma once

// MiniDPDK shim for DPDK's <rte_memory.h>. Only the subset the ENA driver
// references is provided.

// NUMA socket id meaning "any socket". The unikernel has a single NUMA domain,
// so this is the only socket id that matters. Value matches DPDK.
#define SOCKET_ID_ANY (-1)

// Cacheline size used for DMA-friendly alignment (OSv targets a 64-byte line,
// see CACHELINE_ALIGNED in arch/x64/arch.hh).
#define RTE_CACHE_LINE_SIZE 64
