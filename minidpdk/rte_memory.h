#pragma once

// MiniDPDK shim for DPDK's <rte_memory.h>. Only the subset the ENA driver
// references is provided.

// NUMA socket id meaning "any socket". The unikernel has a single NUMA domain,
// so this is the only socket id that matters. Value matches DPDK.
#define SOCKET_ID_ANY (-1)
