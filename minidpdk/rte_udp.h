#pragma once
/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 1982, 1986, 1990, 1993
 *      The Regents of the University of California.
 * Copyright(c) 2010-2014 Intel Corporation.
 * All rights reserved.
 */

// MiniDPDK shim for DPDK's <rte_udp.h>. Provides the UDP header layout needed by
// <rte_net.h> (its checksum prepare path writes ->dgram_cksum). Only the struct
// is ported.

#include <minidpdk/rte_byteorder.h>
#include <minidpdk/rte_common.h>     // __rte_packed

struct rte_udp_hdr {
    rte_be16_t src_port;    /**< UDP source port. */
    rte_be16_t dst_port;    /**< UDP destination port. */
    rte_be16_t dgram_len;   /**< UDP datagram length */
    rte_be16_t dgram_cksum; /**< UDP datagram checksum */
} __rte_packed;
