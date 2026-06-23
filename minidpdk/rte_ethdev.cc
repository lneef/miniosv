// Public ethdev API: the port-id-indexed entry points apps call. Each resolves
// the numeric port id to its rte_eth_dev (the port-id-indexed rte_eth_dev::store
// slot filled by probe) and forwards to the eth_dev_ops table or burst pointers.

#include <cerrno>
#include <cstring>

#include <minidpdk/ethdev_driver.h>
#include <minidpdk/rte_ether.h>
#include <minidpdk/rte_malloc.h>

namespace {
// Resolve a numeric port id to its device handle, or nullptr if out of range /
// no device registered in that slot.
rte_eth_dev *eth_dev_get(uint16_t port_id) {
  if (port_id >= RTE_MAX_ETHPORTS)
    return nullptr;
  return rte_eth_dev::store[port_id];
}
} // namespace

int rte_eth_dev_configure(uint16_t port_id, uint16_t nb_rx_q, uint16_t nb_tx_q,
                          const rte_eth_conf *dev_conf) {
  auto *dev = eth_dev_get(port_id);
  if (!dev)
    return -ENODEV;
  auto *data = dev->data;
  data->dev_conf = *dev_conf;
  data->nb_rx_queues = nb_rx_q;
  data->nb_tx_queues = nb_tx_q;

  // The driver stores per-queue contexts into these arrays during queue setup.
  data->rx_queues = static_cast<void **>(rte_zmalloc(
      "rx_queues", sizeof(void *) * nb_rx_q, RTE_CACHE_LINE_SIZE));
  data->tx_queues = static_cast<void **>(rte_zmalloc(
      "tx_queues", sizeof(void *) * nb_tx_q, RTE_CACHE_LINE_SIZE));
  if ((nb_rx_q && !data->rx_queues) || (nb_tx_q && !data->tx_queues))
    return -ENOMEM;

  return dev->dev_ops->dev_configure(dev);
}

int rte_eth_dev_info_get(uint16_t port_id, rte_eth_dev_info *dev_info) {
  auto *dev = eth_dev_get(port_id);
  if (!dev)
    return -ENODEV;
  *dev_info = {};
  return dev->dev_ops->dev_infos_get(dev, dev_info);
}

int rte_eth_dev_adjust_nb_rx_tx_desc(uint16_t port_id, uint16_t *nb_rx_desc,
                                     uint16_t *nb_tx_desc) {
  rte_eth_dev_info info;
  int rc = rte_eth_dev_info_get(port_id, &info);
  if (rc)
    return rc;

  auto clamp = [](uint16_t v, const rte_eth_desc_lim &lim) -> uint16_t {
    if (v > lim.nb_max)
      return lim.nb_max;
    if (v < lim.nb_min)
      return lim.nb_min;
    return v;
  };
  *nb_rx_desc = clamp(*nb_rx_desc, info.rx_desc_lim);
  *nb_tx_desc = clamp(*nb_tx_desc, info.tx_desc_lim);
  return 0;
}

int rte_eth_rx_queue_setup(uint16_t port_id, uint16_t rx_queue_id,
                           uint16_t nb_rx_desc, unsigned int socket_id,
                           const rte_eth_rxconf *rx_conf, rte_mempool *mb_pool) {
  auto *dev = eth_dev_get(port_id);
  if (!dev)
    return -ENODEV;
  return dev->dev_ops->rx_queue_setup(dev, rx_queue_id, nb_rx_desc, socket_id,
                                      rx_conf, mb_pool);
}

int rte_eth_tx_queue_setup(uint16_t port_id, uint16_t tx_queue_id,
                           uint16_t nb_tx_desc, unsigned int socket_id,
                           const rte_eth_txconf *tx_conf) {
  auto *dev = eth_dev_get(port_id);
  if (!dev)
    return -ENODEV;
  return dev->dev_ops->tx_queue_setup(dev, tx_queue_id, nb_tx_desc, socket_id,
                                      tx_conf);
}

int rte_eth_dev_start(uint16_t port_id) {
  auto *dev = eth_dev_get(port_id);
  if (!dev)
    return -ENODEV;
  int rc = dev->dev_ops->dev_start(dev);
  if (!rc)
    dev->data->dev_started = 1;
  return rc;
}

int rte_eth_dev_stop(uint16_t port_id) {
  auto *dev = eth_dev_get(port_id);
  if (!dev)
    return -ENODEV;
  int rc = dev->dev_ops->dev_stop(dev);
  dev->data->dev_started = 0;
  return rc;
}

int rte_eth_macaddr_get(uint16_t port_id, rte_ether_addr *mac_addr) {
  auto *dev = eth_dev_get(port_id);
  if (!dev)
    return -ENODEV;
  *mac_addr = dev->data->mac_addrs[0];
  return 0;
}

int rte_eth_stats_get(uint16_t port_id, rte_eth_stats *stats) {
  auto *dev = eth_dev_get(port_id);
  if (!dev)
    return -ENODEV;
  *stats = {};
  eth_queue_stats qstats{};
  return dev->dev_ops->stats_get(dev, stats, &qstats);
}

int rte_eth_dev_rss_reta_update(uint16_t port_id,
                                rte_eth_rss_reta_entry64 *reta_conf,
                                uint16_t reta_size) {
  auto *dev = eth_dev_get(port_id);
  if (!dev)
    return -ENODEV;
  return dev->dev_ops->reta_update(dev, reta_conf, reta_size);
}

uint16_t rte_eth_rx_burst(uint16_t port_id, uint16_t queue_id,
                          rte_mbuf **rx_pkts, uint16_t nb_pkts) {
  auto *dev = eth_dev_get(port_id);
  if (!dev)
    return 0;
  return dev->rx_pkt_burst(dev->data->rx_queues[queue_id], rx_pkts, nb_pkts);
}

uint16_t rte_eth_tx_burst(uint16_t port_id, uint16_t queue_id,
                          rte_mbuf **tx_pkts, uint16_t nb_pkts) {
  auto *dev = eth_dev_get(port_id);
  if (!dev)
    return 0;
  return dev->tx_pkt_burst(dev->data->tx_queues[queue_id], tx_pkts, nb_pkts);
}
