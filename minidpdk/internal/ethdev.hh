#pragma once

// MiniDPDK shim for DPDK's ethdev device structure. Defines the core "eth_ops /
// rte_eth_dev" triplet the NIC drivers bind to: the control-path callback table
// (struct eth_dev_ops), the per-device data block (struct rte_eth_dev_data), and
// the device handle (struct rte_eth_dev).
//
// Following the established MiniDPDK convention (see internal/timer_manager.hh and
// internal/mem_pool.hh), the DPDK-named types are plain POD declared at global
// scope so the driver code (compiled as C++) can use them directly. Only the
// structure layout lives here; device registration / probing and the burst inline
// wrappers are intentionally out of scope.

#include <cstdint>

// ---------------------------------------------------------------------------
// Forward declarations for types only ever referenced through pointers. The
// callback typedefs below pass every aggregate parameter by pointer, so these
// need no full definition here.
// ---------------------------------------------------------------------------
struct rte_eth_dev;
struct rte_eth_dev_info;
struct rte_eth_rxconf;
struct rte_eth_txconf;
struct rte_eth_stats;
struct eth_queue_stats;
struct rte_eth_xstat;
struct rte_eth_xstat_name;
struct rte_eth_rss_reta_entry64;
struct rte_ether_addr;
struct rte_mempool;
struct rte_mbuf;

// Maximum number of Rx/Tx queues tracked per port (mirrors DPDK's
// RTE_MAX_QUEUES_PER_PORT; sized to comfortably cover the drivers' own limits).
inline constexpr unsigned RTE_MAX_QUEUES_PER_PORT = 1024;

// ---------------------------------------------------------------------------
// Control-path callback typedefs (one per eth_dev_ops field).
// ---------------------------------------------------------------------------
using eth_dev_configure_t = int (*)(struct rte_eth_dev *dev);
using eth_dev_infos_get_t  = int (*)(struct rte_eth_dev *dev,
                                     struct rte_eth_dev_info *dev_info);
using eth_rx_queue_setup_t = int (*)(struct rte_eth_dev *dev, uint16_t rx_queue_id,
                                     uint16_t nb_rx_desc, unsigned int socket_id,
                                     const struct rte_eth_rxconf *rx_conf,
                                     struct rte_mempool *mb_pool);
using eth_tx_queue_setup_t = int (*)(struct rte_eth_dev *dev, uint16_t tx_queue_id,
                                     uint16_t nb_tx_desc, unsigned int socket_id,
                                     const struct rte_eth_txconf *tx_conf);
using eth_dev_start_t = int (*)(struct rte_eth_dev *dev);
using eth_dev_stop_t  = int (*)(struct rte_eth_dev *dev);
using eth_link_update_t = int (*)(struct rte_eth_dev *dev, int wait_to_complete);
using eth_stats_get_t = int (*)(struct rte_eth_dev *dev, struct rte_eth_stats *stats,
                                struct eth_queue_stats *qstats);
using eth_xstats_get_names_t = int (*)(struct rte_eth_dev *dev,
                                       struct rte_eth_xstat_name *xstats_names,
                                       unsigned int size);
using eth_xstats_get_names_by_id_t = int (*)(struct rte_eth_dev *dev,
                                             const uint64_t *ids,
                                             struct rte_eth_xstat_name *xstats_names,
                                             unsigned int size);
using eth_xstats_get_t = int (*)(struct rte_eth_dev *dev, struct rte_eth_xstat *stats,
                                 unsigned int n);
using eth_xstats_get_by_id_t = int (*)(struct rte_eth_dev *dev, const uint64_t *ids,
                                       uint64_t *values, unsigned int n);
using mtu_set_t = int (*)(struct rte_eth_dev *dev, uint16_t mtu);
using eth_queue_release_t = void (*)(struct rte_eth_dev *dev, uint16_t queue_id);
using eth_dev_close_t = int (*)(struct rte_eth_dev *dev);
using eth_dev_reset_t = int (*)(struct rte_eth_dev *dev);
using reta_update_t = int (*)(struct rte_eth_dev *dev,
                              struct rte_eth_rss_reta_entry64 *reta_conf,
                              uint16_t reta_size);
using reta_query_t = int (*)(struct rte_eth_dev *dev,
                             struct rte_eth_rss_reta_entry64 *reta_conf,
                             uint16_t reta_size);
using eth_rx_enable_intr_t  = int (*)(struct rte_eth_dev *dev, uint16_t rx_queue_id);
using eth_rx_disable_intr_t = int (*)(struct rte_eth_dev *dev, uint16_t rx_queue_id);
using rss_hash_update_t   = int (*)(struct rte_eth_dev *dev,
                                    struct rte_eth_rss_conf *rss_conf);
using rss_hash_conf_get_t = int (*)(struct rte_eth_dev *dev,
                                    struct rte_eth_rss_conf *rss_conf);
using eth_tx_done_cleanup_t = int (*)(void *txq, uint32_t free_cnt);

// ---------------------------------------------------------------------------
// Fast-path burst typedefs.
// ---------------------------------------------------------------------------
using eth_rx_burst_t = uint16_t (*)(void *rxq, struct rte_mbuf **rx_pkts,
                                    uint16_t nb_pkts);
using eth_tx_burst_t = uint16_t (*)(void *txq, struct rte_mbuf **tx_pkts,
                                    uint16_t nb_pkts);
using eth_tx_prep_t  = uint16_t (*)(void *txq, struct rte_mbuf **tx_pkts,
                                    uint16_t nb_pkts);

// ---------------------------------------------------------------------------
// Control-path operations table. Field order matches the driver's designated
// initialiser (C++20 requires designators in declaration order), see
// driver/ena/ena_ethdev.c:344.
// ---------------------------------------------------------------------------
struct eth_dev_ops {
  eth_dev_configure_t          dev_configure;
  eth_dev_infos_get_t          dev_infos_get;
  eth_rx_queue_setup_t         rx_queue_setup;
  eth_tx_queue_setup_t         tx_queue_setup;
  eth_dev_start_t              dev_start;
  eth_dev_stop_t               dev_stop;
  eth_link_update_t            link_update;
  eth_stats_get_t              stats_get;
  eth_xstats_get_names_t       xstats_get_names;
  eth_xstats_get_names_by_id_t xstats_get_names_by_id;
  eth_xstats_get_t             xstats_get;
  eth_xstats_get_by_id_t       xstats_get_by_id;
  mtu_set_t                    mtu_set;
  eth_queue_release_t          rx_queue_release;
  eth_queue_release_t          tx_queue_release;
  eth_dev_close_t              dev_close;
  eth_dev_reset_t              dev_reset;
  reta_update_t                reta_update;
  reta_query_t                 reta_query;
  eth_rx_enable_intr_t         rx_queue_intr_enable;
  eth_rx_disable_intr_t        rx_queue_intr_disable;
  rss_hash_update_t            rss_hash_update;
  rss_hash_conf_get_t          rss_hash_conf_get;
  eth_tx_done_cleanup_t        tx_done_cleanup;
};

// ---------------------------------------------------------------------------
// Types embedded by value in rte_eth_dev_data. Kept minimal: only the members
// the drivers actually read are modelled.
// ---------------------------------------------------------------------------
struct rte_eth_rss_conf {
  uint8_t *rss_key;      // key material; NULL for default
  uint8_t rss_key_len;   // key length in bytes
  uint64_t rss_hf;       // hash functions to apply (RTE_ETH_RSS_*)
};

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

struct rte_eth_link {
  uint32_t link_speed;        // link speed in Mbps (RTE_ETH_SPEED_NUM_*)
  uint16_t link_duplex : 1;   // RTE_ETH_LINK_[HALF/FULL]_DUPLEX
  uint16_t link_autoneg : 1;  // RTE_ETH_LINK_[FIXED/AUTONEG]
  uint16_t link_status : 1;   // RTE_ETH_LINK_[DOWN/UP]
};

// ---------------------------------------------------------------------------
// Per-device data block. Holds the fields the drivers touch through
// dev->data->...; embeds dev_conf and dev_link by value.
// ---------------------------------------------------------------------------
struct rte_eth_dev_data {
  void *dev_private;            // driver-private adapter (struct ena_adapter *)
  uint16_t port_id;

  uint16_t nb_rx_queues;
  uint16_t nb_tx_queues;
  void **rx_queues;            // array of per-queue contexts
  void **tx_queues;

  struct rte_eth_conf dev_conf; // applied device configuration
  struct rte_eth_link dev_link; // current link state

  struct rte_ether_addr *mac_addrs;

  uint8_t rx_queue_state[RTE_MAX_QUEUES_PER_PORT];
  uint8_t tx_queue_state[RTE_MAX_QUEUES_PER_PORT];

  uint32_t dev_flags;           // RTE_ETH_DEV_* flags
  uint8_t dev_started;          // device has been started
  uint8_t scattered_rx;         // Rx of scattered packets is enabled
};

// ---------------------------------------------------------------------------
// Device handle. dev_ops points at the control-path table; the burst pointers
// are wired directly by the driver at init time (see ena_ethdev.c).
// ---------------------------------------------------------------------------
struct rte_eth_dev {
  struct rte_eth_dev_data *data;
  const struct eth_dev_ops *dev_ops;
  eth_rx_burst_t rx_pkt_burst;
  eth_tx_burst_t tx_pkt_burst;
  eth_tx_prep_t  tx_pkt_prepare;
};
