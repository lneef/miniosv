#pragma once

// MiniDPDK shim for DPDK's <rte_net.h>. Only the inline TX checksum/TSO prepare
// path is ported -- the one the ENA driver uses
// (rte_net_intel_cksum_flags_prepare). The non-inline rte_net_get_ptype /
// rte_net_skip_ip6_ext (and struct rte_net_hdr_lens) are omitted: nothing in the
// driver calls them and their real implementation is DPDK's rte_net.c plus the
// RTE_PTYPE_* machinery.
//
// Divergence from upstream: the outer-tunnel offload branch
// (RTE_MBUF_F_TX_OUTER_*) is dropped because minidpdk::mbuf has no
// outer_l2_len/outer_l3_len fields. The ENA driver never sets outer offloads.

#include <cerrno>
#include <cstdint>

#include <rte_branch_prediction.h>   // unlikely
#include <rte_ip.h>
#include <rte_mbuf.h>
#include <rte_tcp.h>
#include <rte_udp.h>
 
static inline int
rte_net_intel_cksum_flags_prepare(rte_mbuf *m, uint64_t ol_flags)
{
    const uint64_t inner_requests = RTE_MBUF_F_TX_IP_CKSUM | RTE_MBUF_F_TX_L4_MASK |
        RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_UDP_SEG;
    /* Initialise ipv4_hdr to avoid false positive compiler warnings. */
    struct rte_ipv4_hdr *ipv4_hdr = NULL;
    struct rte_ipv6_hdr *ipv6_hdr;
    struct rte_tcp_hdr *tcp_hdr;
    struct rte_udp_hdr *udp_hdr;
    uint64_t inner_l3_offset = m->l2_len;
 
    /*
     * Does packet set any of available offloads?
     * Mainly it is required to avoid fragmented headers check if
     * no offloads are requested.
     */
    if (!(ol_flags & inner_requests))
        return 0;

    /*
     * Check if headers are fragmented.
     * The check could be less strict depending on which offloads are
     * requested and headers to be used, but let's keep it simple.
     */
    if (unlikely(rte_pktmbuf_data_len(m) <
             inner_l3_offset + m->l3_len + m->l4_len))
        return -ENOTSUP;
 
    if (ol_flags & RTE_MBUF_F_TX_IPV4) {
        ipv4_hdr = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *,
                inner_l3_offset);
 
        if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM)
            ipv4_hdr->hdr_checksum = 0;
    }
 
    if ((ol_flags & RTE_MBUF_F_TX_L4_MASK) == RTE_MBUF_F_TX_UDP_CKSUM ||
            (ol_flags & RTE_MBUF_F_TX_UDP_SEG)) {
        if (ol_flags & RTE_MBUF_F_TX_IPV4) {
            udp_hdr = (struct rte_udp_hdr *)((char *)ipv4_hdr +
                    m->l3_len);
            udp_hdr->dgram_cksum = rte_ipv4_phdr_cksum(ipv4_hdr,
                    ol_flags);
        } else {
            ipv6_hdr = rte_pktmbuf_mtod_offset(m,
                struct rte_ipv6_hdr *, inner_l3_offset);
            /* non-TSO udp */
            udp_hdr = rte_pktmbuf_mtod_offset(m,
                    struct rte_udp_hdr *,
                    inner_l3_offset + m->l3_len);
            udp_hdr->dgram_cksum = rte_ipv6_phdr_cksum(ipv6_hdr,
                    ol_flags);
        }
    } else if ((ol_flags & RTE_MBUF_F_TX_L4_MASK) == RTE_MBUF_F_TX_TCP_CKSUM ||
            (ol_flags & RTE_MBUF_F_TX_TCP_SEG)) {
        if (ol_flags & RTE_MBUF_F_TX_IPV4) {
            /* non-TSO tcp or TSO */
            tcp_hdr = (struct rte_tcp_hdr *)((char *)ipv4_hdr +
                    m->l3_len);
            tcp_hdr->cksum = rte_ipv4_phdr_cksum(ipv4_hdr,
                    ol_flags);
        } else {
            ipv6_hdr = rte_pktmbuf_mtod_offset(m,
                struct rte_ipv6_hdr *, inner_l3_offset);
            /* non-TSO tcp or TSO */
            tcp_hdr = rte_pktmbuf_mtod_offset(m,
                    struct rte_tcp_hdr *,
                    inner_l3_offset + m->l3_len);
            tcp_hdr->cksum = rte_ipv6_phdr_cksum(ipv6_hdr,
                    ol_flags);
        }
    }
 
    return 0;
}
 
static inline int
rte_net_intel_cksum_prepare(rte_mbuf *m)
{
    return rte_net_intel_cksum_flags_prepare(m, m->ol_flags);
}
