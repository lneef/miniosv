#pragma once

// MiniDPDK shim for DPDK's <rte_tcp.h>. Provides the TCP header layout needed by
// <rte_net.h> (its checksum prepare path writes ->cksum). Only the struct is
// ported.

#include <cstdint>

#include <minidpdk/rte_byteorder.h>
#include <minidpdk/rte_common.h>     // __rte_packed

struct rte_tcp_hdr {
    rte_be16_t src_port; /**< TCP source port. */
    rte_be16_t dst_port; /**< TCP destination port. */
    rte_be32_t sent_seq; /**< TX data sequence number. */
    rte_be32_t recv_ack; /**< RX data acknowledgment sequence number. */
    uint8_t  data_off;   /**< Data offset. */
    uint8_t  tcp_flags;  /**< TCP flags */
    rte_be16_t rx_win;   /**< RX flow control window. */
    rte_be16_t cksum;    /**< TCP checksum. */
    rte_be16_t tcp_urp;  /**< TCP urgent pointer, if any. */
} __rte_packed;
