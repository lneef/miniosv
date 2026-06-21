#pragma once

// MiniDPDK shim for DPDK's <rte_mempool.h>. The pool type is the
// rte_pktmbuf_pool defined in <rte_mbuf.h>; this header adds the rte_mempool
// alias and the mempool-level free.

#include <minidpdk/rte_mbuf.h>

using rte_mempool = rte_pktmbuf_pool;

void rte_mempool_free(rte_mempool *mp);

uint16_t rte_pktmbuf_data_room_size(rte_mempool* mp);
