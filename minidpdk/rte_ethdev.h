#pragma once

// MiniDPDK shim for DPDK's <rte_ethdev.h>. Provides the RSS flag, hash-function
// and RETA definitions the ENA driver references.

#include <cstdint>

#include <minidpdk/rte_bitops.h>     // RTE_BIT64, RTE_BIT32
// rte_mbuf / rte_mempool aliases used by the wrapper declarations below. Pulled
// here so the names are available wherever this header is included (notably from
// <ethdev_driver.h>, before it includes them itself).
#include <minidpdk/rte_mbuf.h>
#include <minidpdk/rte_mempool.h>

struct rte_ether_addr;               // <rte_ether.h>

/* packet fields */
#define RTE_ETH_RSS_IPV4               RTE_BIT64(2)
#define RTE_ETH_RSS_FRAG_IPV4          RTE_BIT64(3)
#define RTE_ETH_RSS_NONFRAG_IPV4_TCP   RTE_BIT64(4)
#define RTE_ETH_RSS_NONFRAG_IPV4_UDP   RTE_BIT64(5)
#define RTE_ETH_RSS_NONFRAG_IPV4_SCTP  RTE_BIT64(6)
#define RTE_ETH_RSS_NONFRAG_IPV4_OTHER RTE_BIT64(7)
#define RTE_ETH_RSS_IPV6               RTE_BIT64(8)
#define RTE_ETH_RSS_FRAG_IPV6          RTE_BIT64(9)
#define RTE_ETH_RSS_NONFRAG_IPV6_TCP   RTE_BIT64(10)
#define RTE_ETH_RSS_NONFRAG_IPV6_UDP   RTE_BIT64(11)
#define RTE_ETH_RSS_NONFRAG_IPV6_SCTP  RTE_BIT64(12)
#define RTE_ETH_RSS_NONFRAG_IPV6_OTHER RTE_BIT64(13)
#define RTE_ETH_RSS_L2_PAYLOAD         RTE_BIT64(14)
#define RTE_ETH_RSS_IPV6_EX            RTE_BIT64(15)
#define RTE_ETH_RSS_IPV6_TCP_EX        RTE_BIT64(16)
#define RTE_ETH_RSS_IPV6_UDP_EX        RTE_BIT64(17)
#define RTE_ETH_RSS_PORT               RTE_BIT64(18)
#define RTE_ETH_RSS_VXLAN              RTE_BIT64(19)
#define RTE_ETH_RSS_GENEVE             RTE_BIT64(20)
#define RTE_ETH_RSS_NVGRE              RTE_BIT64(21)
#define RTE_ETH_RSS_GTPU               RTE_BIT64(23)
#define RTE_ETH_RSS_ETH                RTE_BIT64(24)
#define RTE_ETH_RSS_S_VLAN             RTE_BIT64(25)
#define RTE_ETH_RSS_C_VLAN             RTE_BIT64(26)
#define RTE_ETH_RSS_ESP                RTE_BIT64(27)
#define RTE_ETH_RSS_AH                 RTE_BIT64(28)
#define RTE_ETH_RSS_L2TPV3             RTE_BIT64(29)
#define RTE_ETH_RSS_PFCP               RTE_BIT64(30)
#define RTE_ETH_RSS_PPPOE              RTE_BIT64(31)
#define RTE_ETH_RSS_ECPRI              RTE_BIT64(32)
#define RTE_ETH_RSS_MPLS               RTE_BIT64(33)
#define RTE_ETH_RSS_IPV4_CHKSUM        RTE_BIT64(34)

#define RTE_ETH_RSS_L4_CHKSUM          RTE_BIT64(35)

#define RTE_ETH_RSS_L2TPV2             RTE_BIT64(36)
#define RTE_ETH_RSS_IPV6_FLOW_LABEL    RTE_BIT64(37)

#define RTE_ETH_RSS_IB_BTH             RTE_BIT64(38)

#define RTE_ETH_RSS_L3_SRC_ONLY        RTE_BIT64(63)
#define RTE_ETH_RSS_L3_DST_ONLY        RTE_BIT64(62)
#define RTE_ETH_RSS_L4_SRC_ONLY        RTE_BIT64(61)
#define RTE_ETH_RSS_L4_DST_ONLY        RTE_BIT64(60)
#define RTE_ETH_RSS_L2_SRC_ONLY        RTE_BIT64(59)
#define RTE_ETH_RSS_L2_DST_ONLY        RTE_BIT64(58)


enum rte_eth_hash_function {
    RTE_ETH_HASH_FUNCTION_DEFAULT = 0,
    RTE_ETH_HASH_FUNCTION_TOEPLITZ,
    RTE_ETH_HASH_FUNCTION_SIMPLE_XOR,
    RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ,
    RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ_SORT,
    RTE_ETH_HASH_FUNCTION_MAX,
};

#define RTE_ETH_HASH_ALGO_TO_CAPA(x) RTE_BIT32(x)
#define RTE_ETH_HASH_ALGO_CAPA_MASK(x) RTE_BIT32(RTE_ETH_HASH_FUNCTION_ ## x)

struct rte_eth_rss_conf {
    uint8_t *rss_key;
    uint8_t rss_key_len;
    uint64_t rss_hf;
    enum rte_eth_hash_function algorithm;
};

#define RTE_ETH_RSS_RETA_SIZE_64  64
#define RTE_ETH_RSS_RETA_SIZE_128 128
#define RTE_ETH_RSS_RETA_SIZE_256 256
#define RTE_ETH_RSS_RETA_SIZE_512 512
#define RTE_ETH_RETA_GROUP_SIZE   64

#define RTE_ETH_VMDQ_NUM_UC_HASH_ARRAY 128
#define RTE_ETH_VMDQ_ACCEPT_UNTAG      RTE_BIT32(0)
#define RTE_ETH_VMDQ_ACCEPT_HASH_MC    RTE_BIT32(1)
#define RTE_ETH_VMDQ_ACCEPT_HASH_UC    RTE_BIT32(2)
#define RTE_ETH_VMDQ_ACCEPT_BROADCAST  RTE_BIT32(3)
#define RTE_ETH_VMDQ_ACCEPT_MULTICAST  RTE_BIT32(4)
struct rte_eth_rss_reta_entry64 {
    uint64_t mask;
    uint16_t reta[RTE_ETH_RETA_GROUP_SIZE];
};

// ---------------------------------------------------------------------------
// Constants.
// ---------------------------------------------------------------------------
inline constexpr unsigned RTE_ETHDEV_QUEUE_STAT_CNTRS = 16;
inline constexpr unsigned RTE_ETH_XSTATS_NAME_SIZE = 64;

// Rx multi-queue mode flag tested against dev_conf.rxmode.mq_mode.
#define RTE_ETH_MQ_RX_RSS_FLAG 0x1
// Rx multi-queue RSS mode (value apps assign to rxmode.mq_mode).
#define RTE_ETH_MQ_RX_RSS RTE_ETH_MQ_RX_RSS_FLAG

// ---------------------------------------------------------------------------
// Enums.
// ---------------------------------------------------------------------------
// Per-queue start/stop state stored in rte_eth_dev_data::{rx,tx}_queue_state.
enum queue_state {
  RTE_ETH_QUEUE_STATE_STOPPED = 0,
  RTE_ETH_QUEUE_STATE_STARTED,
};

// Port error-handling mode (rte_eth_dev_info::err_handle_mode).
enum rte_eth_err_handle_mode {
  RTE_ETH_ERROR_HANDLE_MODE_NONE = 0,
  RTE_ETH_ERROR_HANDLE_MODE_PASSIVE,
  RTE_ETH_ERROR_HANDLE_MODE_PROACTIVE,
};

// ---------------------------------------------------------------------------
// Device configuration (rte_eth_conf and its members). rte_eth_rss_conf is
// defined above.
// ---------------------------------------------------------------------------
struct rte_eth_rxmode {
  uint32_t mq_mode;      // multi-queue mode (enum rte_eth_rx_mq_mode)
  uint64_t offloads;     // per-port Rx offloads (RTE_ETH_RX_OFFLOAD_*)
};

struct rte_eth_txmode {
  uint32_t mq_mode;      // multi-queue mode (enum rte_eth_tx_mq_mode)
  uint64_t offloads;     // per-port Tx offloads (RTE_ETH_TX_OFFLOAD_*)
};

struct rte_eth_conf {
  struct rte_eth_rxmode rxmode;
  struct rte_eth_txmode txmode;
  struct {
    struct rte_eth_rss_conf rss_conf;
  } rx_adv_conf;
  struct {
    uint32_t lsc : 1;    // link status change interrupt
    uint32_t rxq : 1;    // Rx queue interrupt
  } intr_conf;
};

// ---------------------------------------------------------------------------
// Link state.
// ---------------------------------------------------------------------------
struct rte_eth_link {
  uint32_t link_speed;        // link speed in Mbps (RTE_ETH_SPEED_NUM_*)
  uint16_t link_duplex : 1;   // RTE_ETH_LINK_[HALF/FULL]_DUPLEX
  uint16_t link_autoneg : 1;  // RTE_ETH_LINK_[FIXED/AUTONEG]
  uint16_t link_status : 1;   // RTE_ETH_LINK_[DOWN/UP]
};

// ---------------------------------------------------------------------------
// Per-queue Rx/Tx setup parameters (rx_queue_setup / tx_queue_setup).
// ---------------------------------------------------------------------------
struct rte_eth_rxconf {
  uint64_t offloads;        // per-queue Rx offloads (RTE_ETH_RX_OFFLOAD_*)
  uint16_t rx_free_thresh;  // Rx descriptor free threshold
};

struct rte_eth_txconf {
  uint64_t offloads;        // per-queue Tx offloads (RTE_ETH_TX_OFFLOAD_*)
  uint16_t tx_free_thresh;  // Tx descriptor free threshold
};

// ---------------------------------------------------------------------------
// Device information (dev_infos_get).
// ---------------------------------------------------------------------------
struct rte_eth_desc_lim {
  uint16_t nb_max;          // max descriptors per ring
  uint16_t nb_min;          // min descriptors per ring
  uint16_t nb_seg_max;      // max segments per packet
  uint16_t nb_mtu_seg_max;  // max segments per MTU-sized packet
};

struct rte_eth_dev_portconf {
  uint16_t burst_size;
  uint16_t ring_size;
  uint16_t nb_queues;
};

struct rte_eth_dev_info {
  uint16_t min_mtu;
  uint16_t max_mtu;
  uint32_t min_rx_bufsize;
  uint32_t max_rx_pktlen;
  uint16_t max_rx_queues;
  uint16_t max_tx_queues;
  uint16_t max_mac_addrs;
  uint8_t  hash_key_size;
  uint16_t reta_size;
  uint32_t speed_capa;                 // RTE_ETH_LINK_SPEED_* capabilities
  uint64_t flow_type_rss_offloads;     // supported RSS hash types
  uint64_t rx_offload_capa;
  uint64_t tx_offload_capa;
  uint64_t rx_queue_offload_capa;
  uint64_t tx_queue_offload_capa;
  enum rte_eth_err_handle_mode err_handle_mode;
  struct rte_eth_desc_lim rx_desc_lim;
  struct rte_eth_desc_lim tx_desc_lim;
  struct rte_eth_dev_portconf default_rxportconf;
  struct rte_eth_dev_portconf default_txportconf;
};

// ---------------------------------------------------------------------------
// Statistics (stats_get / xstats_get).
// ---------------------------------------------------------------------------
struct rte_eth_stats {
  uint64_t ipackets;
  uint64_t opackets;
  uint64_t ibytes;
  uint64_t obytes;
  uint64_t imissed;
  uint64_t ierrors;
  uint64_t oerrors;
  uint64_t rx_nombuf;
};

// Per-queue statistics: the second stats_get argument. DPDK keeps these arrays
// inside rte_eth_stats; this shim splits them out, matching eth_stats_get_t.
struct eth_queue_stats {
  uint64_t q_ipackets[RTE_ETHDEV_QUEUE_STAT_CNTRS];
  uint64_t q_opackets[RTE_ETHDEV_QUEUE_STAT_CNTRS];
  uint64_t q_ibytes[RTE_ETHDEV_QUEUE_STAT_CNTRS];
  uint64_t q_obytes[RTE_ETHDEV_QUEUE_STAT_CNTRS];
  uint64_t q_errors[RTE_ETHDEV_QUEUE_STAT_CNTRS];
};

struct rte_eth_xstat {
  uint64_t id;
  uint64_t value;
};

struct rte_eth_xstat_name {
  char name[RTE_ETH_XSTATS_NAME_SIZE];
};

enum rte_eth_event_type {
  RTE_ETH_EVENT_INTR_LSC,
  RTE_ETH_EVENT_INTR_RESET,
};

// ---------------------------------------------------------------------------
// Public ethdev API. Each resolves a numeric port id to its device handle and
// forwards to the driver's eth_dev_ops table or burst pointers. Definitions in
// rte_ethdev.cc. Bad port ids return -ENODEV.
// ---------------------------------------------------------------------------
int rte_eth_dev_configure(uint16_t port_id, uint16_t nb_rx_q, uint16_t nb_tx_q,
                          const struct rte_eth_conf *dev_conf);
int rte_eth_dev_info_get(uint16_t port_id, struct rte_eth_dev_info *dev_info);
int rte_eth_dev_adjust_nb_rx_tx_desc(uint16_t port_id, uint16_t *nb_rx_desc,
                                     uint16_t *nb_tx_desc);
int rte_eth_rx_queue_setup(uint16_t port_id, uint16_t rx_queue_id,
                           uint16_t nb_rx_desc, unsigned int socket_id,
                           const struct rte_eth_rxconf *rx_conf,
                           rte_mempool *mb_pool);
int rte_eth_tx_queue_setup(uint16_t port_id, uint16_t tx_queue_id,
                           uint16_t nb_tx_desc, unsigned int socket_id,
                           const struct rte_eth_txconf *tx_conf);
int rte_eth_dev_start(uint16_t port_id);
int rte_eth_dev_stop(uint16_t port_id);
int rte_eth_macaddr_get(uint16_t port_id, struct rte_ether_addr *mac_addr);
int rte_eth_stats_get(uint16_t port_id, struct rte_eth_stats *stats);
int rte_eth_dev_rss_reta_update(uint16_t port_id,
                                struct rte_eth_rss_reta_entry64 *reta_conf,
                                uint16_t reta_size);
uint16_t rte_eth_rx_burst(uint16_t port_id, uint16_t queue_id,
                          rte_mbuf **rx_pkts, uint16_t nb_pkts);
uint16_t rte_eth_tx_burst(uint16_t port_id, uint16_t queue_id,
                          rte_mbuf **tx_pkts, uint16_t nb_pkts);
