#pragma once
/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 1982, 1986, 1990, 1993
 *      The Regents of the University of California.
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright(c) 2014 6WIND S.A.
 * All rights reserved.
 */

// MiniDPDK shim for DPDK's <rte_ip.h>. Provides the IPv4/IPv6 header layouts and
// the pseudo-header / raw checksum helpers needed by <rte_net.h> (and the IPv4
// header fields the ENA driver reads directly). Only that subset is ported.

#include <cstdint>
#include <cstring>

#include <minidpdk/rte_byteorder.h>
#include <minidpdk/rte_common.h>     // RTE_PTR_ADD, RTE_ALIGN_FLOOR
#include <minidpdk/rte_branch_prediction.h>   // unlikely
#include <minidpdk/rte_mbuf.h>                // RTE_MBUF_F_TX_* used by the phdr helpers

#define RTE_IPV4_HDR_IHL_MASK   (0x0f)
#define RTE_IPV4_IHL_MULTIPLIER (4)

#define RTE_IPPROTO_UDP 17   // UDP protocol number (next_proto_id)

struct rte_ipv4_hdr {
    __extension__
    union {
        uint8_t version_ihl;    /**< version and header length */
        struct {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
            uint8_t ihl : 4;     /**< header length */
            uint8_t version : 4; /**< version */
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
            uint8_t version : 4; /**< version */
            uint8_t ihl : 4;     /**< header length */
#endif
        };
    };
    uint8_t  type_of_service;   /**< type of service */
    rte_be16_t total_length;    /**< length of packet */
    rte_be16_t packet_id;       /**< packet ID */
    rte_be16_t fragment_offset; /**< fragmentation offset */
    uint8_t  time_to_live;      /**< time to live */
    uint8_t  next_proto_id;     /**< protocol ID */
    rte_be16_t hdr_checksum;    /**< header checksum */
    rte_be32_t src_addr;        /**< source address */
    rte_be32_t dst_addr;        /**< destination address */
} __rte_packed;

#define IPVERSION (4)
#define RTE_IPV4_MIN_IHL    (0x5)
#define RTE_IPV4_VHL_DEF    ((IPVERSION << 4) | RTE_IPV4_MIN_IHL)

#define RTE_IPV4_HDR_DF_FLAG 0x4000 /**< "Don't fragment" flag in fragment_offset */

struct rte_ipv6_hdr {
    rte_be32_t vtc_flow;    /**< IP version, traffic class & flow label. */
    rte_be16_t payload_len; /**< IP payload size, including ext. headers */
    uint8_t  proto;         /**< Protocol, next header. */
    uint8_t  hop_limits;    /**< Hop limits. */
    uint8_t  src_addr[16];  /**< IP address of source host. */
    uint8_t  dst_addr[16];  /**< IP address of destination host(s). */
} __rte_packed;

// Return the IPv4 header length in bytes.
static inline uint8_t rte_ipv4_hdr_len(const struct rte_ipv4_hdr *ipv4_hdr)
{
    return (uint8_t)((ipv4_hdr->version_ihl & RTE_IPV4_HDR_IHL_MASK) *
                     RTE_IPV4_IHL_MULTIPLIER);
}

// Process the non-complemented checksum of a buffer, folding @sum in.
static inline uint32_t __rte_raw_cksum(const void *buf, size_t len, uint32_t sum)
{
    const void *end;

    for (end = RTE_PTR_ADD(buf, RTE_ALIGN_FLOOR(len, sizeof(uint16_t)));
         buf != end; buf = RTE_PTR_ADD(buf, sizeof(uint16_t))) {
        uint16_t v;

        memcpy(&v, buf, sizeof(uint16_t));
        sum += v;
    }

    /* if length is odd, keeping it byte order independent */
    if (unlikely(len % 2)) {
        uint16_t left = 0;

        memcpy(&left, end, 1);
        sum += left;
    }

    return sum;
}

// Reduce a raw checksum accumulator to a 16-bit value.
static inline uint16_t __rte_raw_cksum_reduce(uint32_t sum)
{
    sum = ((sum & 0xffff0000) >> 16) + (sum & 0xffff);
    sum = ((sum & 0xffff0000) >> 16) + (sum & 0xffff);
    return (uint16_t)sum;
}

// Process the non-complemented checksum of a buffer.
static inline uint16_t rte_raw_cksum(const void *buf, size_t len)
{
    uint32_t sum;

    sum = __rte_raw_cksum(buf, len, 0);
    return __rte_raw_cksum_reduce(sum);
}

// Compute the IPv4 pseudo-header checksum used for L4 offload.
static inline uint16_t rte_ipv4_phdr_cksum(const struct rte_ipv4_hdr *ipv4_hdr,
                                           uint64_t ol_flags)
{
    struct ipv4_psd_header {
        uint32_t src_addr; /* IP address of source host. */
        uint32_t dst_addr; /* IP address of destination host. */
        uint8_t  zero;     /* zero. */
        uint8_t  proto;    /* L4 protocol type. */
        uint16_t len;      /* L4 length. */
    } psd_hdr;

    uint32_t l3_len;

    psd_hdr.src_addr = ipv4_hdr->src_addr;
    psd_hdr.dst_addr = ipv4_hdr->dst_addr;
    psd_hdr.zero = 0;
    psd_hdr.proto = ipv4_hdr->next_proto_id;
    if (ol_flags & (RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_UDP_SEG)) {
        psd_hdr.len = 0;
    } else {
        l3_len = rte_be_to_cpu_16(ipv4_hdr->total_length);
        psd_hdr.len = rte_cpu_to_be_16((uint16_t)(l3_len -
                                                  rte_ipv4_hdr_len(ipv4_hdr)));
    }
    return rte_raw_cksum(&psd_hdr, sizeof(psd_hdr));
}

// Compute the IPv6 pseudo-header checksum used for L4 offload.
static inline uint16_t rte_ipv6_phdr_cksum(const struct rte_ipv6_hdr *ipv6_hdr,
                                           uint64_t ol_flags)
{
    uint32_t sum;
    struct {
        rte_be32_t len;   /* L4 length. */
        rte_be32_t proto; /* L4 protocol - top 3 bytes must be zero */
    } psd_hdr;

    psd_hdr.proto = (uint32_t)(ipv6_hdr->proto << 24);
    if (ol_flags & (RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_UDP_SEG)) {
        psd_hdr.len = 0;
    } else {
        psd_hdr.len = ipv6_hdr->payload_len;
    }

    sum = __rte_raw_cksum(ipv6_hdr->src_addr,
                          sizeof(ipv6_hdr->src_addr) + sizeof(ipv6_hdr->dst_addr),
                          0);
    sum = __rte_raw_cksum(&psd_hdr, sizeof(psd_hdr), sum);
    return __rte_raw_cksum_reduce(sum);
}
