#pragma once
/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

// MiniDPDK shim for DPDK's <rte_ether.h>. Provides the Ethernet address type,
// the related length constants and the address-manipulation helpers. Only the
// subset the ENA driver references is provided.

#include <cstdint>

#include <minidpdk/rte_byteorder.h>   // rte_be16_t
#include <minidpdk/rte_common.h>      // __rte_packed

#define RTE_ETHER_ADDR_LEN 6   // octets in an Ethernet address
#define RTE_ETHER_HDR_LEN  14  // octets in an Ethernet header (no VLAN tag)
#define RTE_ETHER_CRC_LEN  4   // octets in the Ethernet frame check sequence

#define RTE_ETHER_TYPE_IPV4 0x0800   // IPv4 EtherType

// 48-bit Ethernet (MAC) address. Matches DPDK's layout so the driver's casts
// from raw mac byte buffers stay valid.
struct rte_ether_addr {
    uint8_t addr_bytes[RTE_ETHER_ADDR_LEN];
};

// Ethernet header (no VLAN tag). Layout matches DPDK.
struct rte_ether_hdr {
    struct rte_ether_addr dst_addr;
    struct rte_ether_addr src_addr;
    rte_be16_t ether_type;
} __rte_packed;

// Copy an Ethernet address from `ea_from` to `ea_to`.
inline void rte_ether_addr_copy(const struct rte_ether_addr *ea_from,
                                struct rte_ether_addr *ea_to) {
    *ea_to = *ea_from;
}
