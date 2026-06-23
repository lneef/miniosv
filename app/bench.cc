// MiniDPDK microbench: a UDP ping/pong/receive packet benchmark, ported from
// OSv's modules/microbench. It drives the NIC through the public rte_eth_*
// shim API. Linked statically into the kernel as the default app; the kernel
// calls osv_app_main() once after early init.
//
// RX is poll-only here (MiniDPDK has no app-facing RX-interrupt callback): every
// mode receives via rte_eth_rx_burst. The packet build/verify helpers (formerly
// modules/microbench/net.hh) are folded in below.

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <csignal>
#include <endian.h>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <osv/power.hh>

#include <minidpdk/defs.hh>               // RTE_ETH_{RX,TX}_OFFLOAD_*_CKSUM
#include <minidpdk/internal/mem_pool.hh>  // minidpdk::mem_pool::kMaxDataLen
// rte_mbuf.h / rte_mempool.h first: establish the rte_mbuf / rte_mempool type
// aliases before <rte_ethdev.h>, whose wrapper decls reference them.
#include <minidpdk/rte_mbuf.h>
#include <minidpdk/rte_mempool.h>
#include <minidpdk/rte_cycles.h>
#include <minidpdk/rte_ethdev.h>
#include <minidpdk/rte_ether.h>
#include <minidpdk/rte_ip.h>
#include <minidpdk/rte_lcore.h>
#include <minidpdk/rte_udp.h>

#define SWAP(val1, val2)                                                       \
  do {                                                                         \
    auto temp = val1;                                                          \
    val1 = val2;                                                               \
    val2 = temp;                                                               \
  } while (0);

static constexpr uint8_t TTL = 64;

using pkt_t = rte_mbuf;
struct payload {
  uint64_t ticks;
};

static volatile int terminate = 0;
static void handler(int sig) {
  (void)sig;
  terminate = 1;
}

// ---------------------------------------------------------------------------
// Packet build / verify (formerly net.hh).
// ---------------------------------------------------------------------------
struct app_config {
  rte_ether_addr src;
  rte_ether_addr dst;
  uint32_t sip;
  uint32_t dip;
  uint32_t l4port;
  uint32_t mtu = 128;
};

static void create_packet(const app_config &config, rte_mbuf *pkt) {
  uint16_t len = config.mtu - sizeof(rte_udp_hdr) - sizeof(rte_ipv4_hdr);
  rte_ether_hdr *eth = rte_pktmbuf_mtod(pkt, rte_ether_hdr *);
  rte_ipv4_hdr *ipv4 = reinterpret_cast<rte_ipv4_hdr *>(eth + 1);
  rte_udp_hdr *udp = reinterpret_cast<rte_udp_hdr *>(ipv4 + 1);

  len += sizeof(*udp);
  udp->src_port = htobe16(config.l4port);
  udp->dst_port = htobe16(config.l4port);
  udp->dgram_len = htobe16(len);
  pkt->l4_len = sizeof(*udp);

  len += sizeof(*ipv4);
  ipv4->version_ihl = RTE_IPV4_VHL_DEF;
  ipv4->time_to_live = TTL;
  ipv4->next_proto_id = RTE_IPPROTO_UDP;
  ipv4->fragment_offset = 0;
  ipv4->packet_id = 0;
  ipv4->total_length = htobe16(len);
  ipv4->type_of_service = 0;
  ipv4->dst_addr = config.dip;
  ipv4->src_addr = config.sip;
  pkt->l3_len = sizeof(*ipv4);

  eth->src_addr = config.src;
  eth->dst_addr = config.dst;
  eth->ether_type = htobe16(RTE_ETHER_TYPE_IPV4);
  pkt->l2_len = sizeof(*eth);

  pkt->pkt_len = sizeof(*eth) + len;
  pkt->data_len = sizeof(*eth) + len;
  pkt->nb_segs = 1;
  udp->dgram_cksum = 0;
  ipv4->hdr_checksum = 0;
  pkt->ol_flags =
      RTE_MBUF_F_TX_IP_CKSUM | RTE_MBUF_F_TX_UDP_CKSUM | RTE_MBUF_F_TX_IPV4;
}

static bool verify_packet(rte_mbuf *pkt) {
  auto *eth = rte_pktmbuf_mtod(pkt, rte_ether_hdr *);
  if (eth->ether_type != htobe16(RTE_ETHER_TYPE_IPV4))
    return false;
  bool l3_valid = (pkt->ol_flags & RTE_MBUF_F_RX_IP_CKSUM_GOOD) ||
                  ((pkt->ol_flags & RTE_MBUF_F_RX_IP_CKSUM_MASK) ==
                   RTE_MBUF_F_RX_IP_CKSUM_UNKNOWN);
  bool l4_valid = (pkt->ol_flags & RTE_MBUF_F_RX_L4_CKSUM_GOOD) ||
                  ((pkt->ol_flags & RTE_MBUF_F_RX_L4_CKSUM_MASK) ==
                   RTE_MBUF_F_RX_L4_CKSUM_UNKNOWN);
  return l3_valid && l4_valid;
}

template <typename T> static __inline T pun(rte_mbuf *pbuf) {
  char *data = rte_pktmbuf_mtod(pbuf, char *) + sizeof(rte_ipv4_hdr) +
               sizeof(rte_udp_hdr) + sizeof(rte_ether_hdr);
  T ret_data;
  std::memcpy(&ret_data, data, sizeof(T));
  return ret_data;
}

template <typename T> static __inline void move_data(rte_mbuf *pbuf, T &data) {
  char *data_ptr = rte_pktmbuf_mtod(pbuf, char *) + sizeof(rte_ipv4_hdr) +
                   sizeof(rte_udp_hdr) + sizeof(rte_ether_hdr);
  memcpy(data_ptr, &data, sizeof(T));
}

using pool_ptr = std::unique_ptr<rte_pktmbuf_pool, decltype(&rte_mempool_free)>;

enum class opmode { PING, PONG, RECEIVE };

struct benchmark_config {
  app_config app;
  uint64_t rt = 30;
  uint16_t burst_size = 32;
  uint16_t nb_cores = 1;
  opmode role = opmode::PONG;
};

struct capabilities {
  bool ip_cksum_tx = false, ip_cksum_rx = false;
  bool l4_cksum_tx = false, l4_cksum_rx = false;
};

struct thread_block {
  uint16_t rx_queue = 0;
  uint16_t tx_queue = 0;
  pool_ptr pool;
  uint64_t ticks = 0, pkts = 0, faulty = 0;
  thread_block() : pool(nullptr, &rte_mempool_free) {}
};

struct port_info {
  uint16_t port_id = 0;
  capabilities caps;
  rte_ether_addr addr{};
  std::vector<thread_block> thread_blocks;

  thread_block &local() {
    return thread_blocks[rte_lcore_index(rte_lcore_id())];
  }
};

struct lcore_adapter {
  port_info &info;
  benchmark_config &config;
};

static uint8_t RSS_DEFAULT_KEY[] = {
    0xbe, 0xac, 0x01, 0xfa, 0x6a, 0x42, 0xb7, 0x3b, 0x80, 0x30,
    0xf2, 0x0c, 0x77, 0xcb, 0x2d, 0xa3, 0xae, 0x7b, 0x30, 0xb4,
    0xd0, 0xca, 0x2b, 0xcb, 0x43, 0xa3, 0x8f, 0xb0, 0x41, 0x67,
    0x25, 0x3d, 0x25, 0x5b, 0x0e, 0xc2, 0x6d, 0x5a, 0x56, 0xda};
static constexpr unsigned RSS_KEY_LEN = 40;

static void setup_reta(port_info &info, uint32_t nrx, uint32_t reta_size) {
  if (reta_size == 0)
    return;
  auto groups =
      (reta_size + RTE_ETH_RETA_GROUP_SIZE - 1) / RTE_ETH_RETA_GROUP_SIZE;
  std::vector<rte_eth_rss_reta_entry64> reta(groups);
  for (auto i = 0u; i < reta_size; ++i) {
    uint32_t reta_id = i / RTE_ETH_RETA_GROUP_SIZE;
    uint32_t reta_pos = i % RTE_ETH_RETA_GROUP_SIZE;
    reta[reta_id].mask = UINT64_MAX;
    reta[reta_id].reta[reta_pos] = static_cast<uint16_t>(i % nrx);
  }
  if (rte_eth_dev_rss_reta_update(info.port_id, reta.data(), reta_size))
    std::cout << "reta update failed" << std::endl;
}

static int configure_port(port_info &info, benchmark_config &config) {
  static constexpr uint16_t kDefaultDescNum = 1024;
  static constexpr uint32_t kMempoolCacheSize = 256;
  uint16_t nb_cores = config.nb_cores;
  rte_eth_dev_info dinfo{};
  rte_eth_rxconf rxconf{};
  rte_eth_txconf txconf{};
  rte_eth_conf conf{};

  if (rte_eth_dev_info_get(info.port_id, &dinfo)) {
    std::cout << "no dev" << std::endl;
    return ENODEV;
  }

  uint16_t rx_desc =
      std::min<uint16_t>(kDefaultDescNum, dinfo.rx_desc_lim.nb_max);
  uint16_t tx_desc =
      std::min<uint16_t>(kDefaultDescNum, dinfo.tx_desc_lim.nb_max);
  rte_eth_dev_adjust_nb_rx_tx_desc(info.port_id, &rx_desc, &tx_desc);

  if (dinfo.tx_offload_capa & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM)
    conf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_IPV4_CKSUM;
  if (dinfo.tx_offload_capa & RTE_ETH_TX_OFFLOAD_UDP_CKSUM)
    conf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_UDP_CKSUM;
  if (dinfo.rx_offload_capa & RTE_ETH_RX_OFFLOAD_IPV4_CKSUM)
    conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_IPV4_CKSUM;
  if (dinfo.rx_offload_capa & RTE_ETH_RX_OFFLOAD_UDP_CKSUM)
    conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_UDP_CKSUM;

  info.caps.ip_cksum_tx = dinfo.tx_offload_capa & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM;
  info.caps.l4_cksum_tx = dinfo.tx_offload_capa & RTE_ETH_TX_OFFLOAD_UDP_CKSUM;
  info.caps.ip_cksum_rx = dinfo.rx_offload_capa & RTE_ETH_RX_OFFLOAD_IPV4_CKSUM;
  info.caps.l4_cksum_rx = dinfo.rx_offload_capa & RTE_ETH_RX_OFFLOAD_UDP_CKSUM;

  bool rss = nb_cores > 1;
  auto &rssconf = conf.rx_adv_conf.rss_conf;
  if (rss) {
    conf.rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;
    rssconf.algorithm = RTE_ETH_HASH_FUNCTION_DEFAULT;
    if (dinfo.hash_key_size == RSS_KEY_LEN) {
      rssconf.rss_key = RSS_DEFAULT_KEY;
      rssconf.rss_key_len = RSS_KEY_LEN;
    }
  } else {
    rssconf.rss_key = nullptr;
    rssconf.rss_hf = 0;
  }
  if (rte_eth_dev_configure(info.port_id, nb_cores, nb_cores, &conf)) {
    std::cout << "dev configure failed" << std::endl;
    return 1;
  }

  rxconf.offloads = conf.rxmode.offloads;
  rxconf.rx_free_thresh = config.burst_size;
  txconf.offloads = conf.txmode.offloads;

  info.thread_blocks.resize(nb_cores);
  for (uint16_t i = 0; i < nb_cores; ++i) {
    auto &tb = info.thread_blocks[i];
    std::string name = "pool-" + std::to_string(i);
    uint32_t pool_sz = static_cast<uint32_t>(2 * rx_desc - 1) +
                       static_cast<uint32_t>(2 * tx_desc - 1);
    tb.pool = pool_ptr(
        rte_pktmbuf_pool_create(name.c_str(), pool_sz, kMempoolCacheSize, 0,
                                minidpdk::mem_pool::kMaxDataLen, 0),
        &rte_mempool_free);
    if (!tb.pool) {
      std::cout << "pool create failed" << std::endl;
      return 1;
    }
    tb.rx_queue = i;
    tb.tx_queue = i;
    if (rte_eth_rx_queue_setup(info.port_id, i, rx_desc, 0, &rxconf,
                               tb.pool.get())) {
      std::cout << "rx queue setup failed" << std::endl;
      return 1;
    }
    if (rte_eth_tx_queue_setup(info.port_id, i, tx_desc, 0, &txconf)) {
      std::cout << "tx queue setup failed" << std::endl;
      return 1;
    }
  }

  rte_eth_macaddr_get(info.port_id, &info.addr);
  config.app.src = info.addr;

  if (rte_eth_dev_start(info.port_id)) {
    std::cout << "Starting dev failed" << std::endl;
    return 1;
  }
  if (rss)
    setup_reta(info, nb_cores, dinfo.reta_size);
  return 0;
}

static void init_packets(const std::vector<rte_mbuf *> &pkts) {
  payload payload{static_cast<uint64_t>(rte_get_timer_cycles())};
  for (auto *pkt : pkts) {
    move_data(pkt, payload);
  }
}

static void close_port(port_info &info) { rte_eth_dev_stop(info.port_id); }

static uint16_t receive_packets_ping(thread_block &tb,
                                     std::vector<rte_mbuf *> &pkts,
                                     uint16_t nb_rx) {
  uint16_t total = 0;
  auto ticks = rte_get_timer_cycles();
  for (uint16_t i = 0; i < nb_rx; ++i) {
    if (!verify_packet(pkts[i])) {
      tb.faulty++;
      continue;
    }

    auto pticks = pun<payload>(pkts[i]);
    auto diff = ticks - pticks.ticks;
    tb.ticks += diff;
    ++tb.pkts;
    ++total;
  }
  rte_pktmbuf_free_bulk(pkts.data(), nb_rx);
  return total;
}

static int receive_packets_pong(const rte_ether_addr &src, rte_mbuf *pkt) {
  if (!verify_packet(pkt))
    return -1;
  rte_ether_hdr *eth = rte_pktmbuf_mtod(pkt, rte_ether_hdr *);
  rte_ipv4_hdr *ipv4 = reinterpret_cast<rte_ipv4_hdr *>(eth + 1);
  rte_udp_hdr *udp = reinterpret_cast<rte_udp_hdr *>(ipv4 + 1);
  eth->dst_addr = eth->src_addr;
  eth->src_addr = src;
  SWAP(ipv4->dst_addr, ipv4->src_addr);
  SWAP(udp->dst_port, udp->src_port);
  udp->dgram_cksum = 0;
  ipv4->hdr_checksum = 0;
  ipv4->time_to_live = TTL;
  pkt->l2_len = sizeof(*eth);
  pkt->l3_len = sizeof(*ipv4);
  pkt->l4_len = sizeof(*udp);
  pkt->ol_flags =
      RTE_MBUF_F_TX_UDP_CKSUM | RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IP_CKSUM;
  return 0;
}

static int lcore_ping(void *arg) {
  auto &[info, config] = *static_cast<lcore_adapter *>(arg);
  auto &tb = info.local();
  uint16_t nb_rx = 0, burst_size = config.burst_size, total = 0;
  uint16_t nb_tx = burst_size;

  std::vector<rte_mbuf *> pkts(burst_size, nullptr);
  std::vector<rte_mbuf *> rpkts(burst_size, nullptr);
  auto cycles = rte_get_timer_cycles();
  auto end = cycles + config.rt * rte_get_timer_hz();
  for (; cycles < end; cycles = rte_get_timer_cycles()) {
    if (rte_pktmbuf_alloc_bulk(tb.pool.get(), pkts.data(), nb_tx)) {
      std::cerr << "not enough buffers" << std::endl;
      continue;
    }
    for (auto *pkt : pkts)
      create_packet(config.app, pkt);
    init_packets(pkts);
    nb_tx =
        rte_eth_tx_burst(info.port_id, tb.tx_queue, pkts.data(), burst_size);
    total = 0;
    do {
      nb_rx = rte_eth_rx_burst(info.port_id, tb.rx_queue, rpkts.data(),
                               burst_size);
      if (nb_rx)
        total += receive_packets_ping(tb, rpkts, nb_rx);
    } while (total < nb_tx && rte_get_timer_cycles() < end);
  }
  return 0;
}

static int lcore_pong(void *arg) {
  auto &[info, config] = *static_cast<lcore_adapter *>(arg);
  auto &tb = info.local();
  uint16_t nb_rx = 0, burst_size = config.burst_size;
  uint16_t nb_tx = burst_size;
  uint16_t nb_rm = 0;
  std::vector<rte_mbuf *> pkts(burst_size, nullptr);
  std::vector<rte_mbuf *> rpkts(burst_size, nullptr);
  for (; !terminate;) {
    nb_rx = rte_eth_rx_burst(info.port_id, tb.rx_queue, rpkts.data(),
                             burst_size - nb_rm);
    for (uint16_t i = 0; i < nb_rx; ++i) {
      pkts[nb_rm] = rpkts[i];
      if (!receive_packets_pong(config.app.src, pkts[nb_rm]))
        ++nb_rm;
    }
    nb_tx = rte_eth_tx_burst(info.port_id, tb.tx_queue, pkts.data(), nb_rm);
    for (uint16_t j = 0, i = nb_tx; i < nb_rm; ++i, ++j)
      pkts[j] = pkts[i];
    nb_rm = nb_rm - nb_tx;
    tb.pkts += nb_tx;
  }
  return 0;
}

static int lcore_recv(void *arg) {
  auto &[info, config] = *static_cast<lcore_adapter *>(arg);
  auto &tb = info.local();
  std::vector<pkt_t *> pkts(config.burst_size, nullptr);
  auto begin = rte_get_timer_cycles();
  for (; !terminate;) {
    auto rx = rte_eth_rx_burst(info.port_id, tb.rx_queue, pkts.data(),
                               config.burst_size);
    if (!rx)
      continue;
    tb.pkts += rx;
    rte_pktmbuf_free_bulk(pkts.data(), rx);
  }
  tb.ticks = rte_get_timer_cycles() - begin;
  return 0;
}

extern "C" void osv_app_main() {
  struct sigaction sa{};
  sa.sa_handler = handler;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  port_info info;
  benchmark_config config;
  auto &conf = config.app;
  // Compile-time defaults (the static-linked app has no argv). Addresses are
  // stored network byte order, like inet_addr would yield: 10.0.0.1 / 10.0.0.2.
  conf.sip = htobe32(0x0A000001);
  conf.dip = htobe32(0x0A000002);
  conf.dst = rte_ether_addr{{0x02, 0x00, 0x00, 0x00, 0x00, 0x02}};
  conf.l4port = 1234;
  conf.mtu = 128;

  lcore_container::init(config.nb_cores);
  if (configure_port(info, config)) {
    osv::poweroff();
    return;
  }

  lcore_adapter adapter{info, config};
  int (*lcore_fn)(void *) = nullptr;
  switch (config.role) {
  case opmode::PING:
    lcore_fn = lcore_ping;
    break;
  case opmode::PONG:
    lcore_fn = lcore_pong;
    break;
  case opmode::RECEIVE:
    lcore_fn = lcore_recv;
    break;
  }

  rte_eth_stats stats;
  rte_eth_stats_get(info.port_id, &stats);
  std::cerr << stats.imissed << ", " << stats.ierrors << ", " << stats.ipackets
            << ", " << stats.ibytes << std::endl;

  rte_eal_mp_remote_launch(lcore_fn, &adapter, CALL_MAIN);
  rte_eal_mp_wait_lcore();

  uint64_t total_ticks = 0, total_pkts = 0, max_ticks = 0;
  for (auto &tb : info.thread_blocks) {
    total_ticks += tb.ticks;
    total_pkts += tb.pkts;
    max_ticks = std::max(max_ticks, tb.ticks);
  }

  auto timer_hz = rte_get_timer_hz();
  std::cout << "Packets:" << total_pkts << std::endl;
  if (config.role == opmode::PING && total_pkts) {
    std::cout << "Latency:"
              << (static_cast<double>(total_ticks) / (timer_hz / 1e6)) /
                     static_cast<double>(total_pkts)
              << std::endl;
    std::cout << "PPS:" << static_cast<double>(total_pkts) / config.rt
              << std::endl;
  } else if (config.role == opmode::RECEIVE && max_ticks) {
    double seconds = static_cast<double>(max_ticks) / timer_hz;
    std::cout << "PPS:" << static_cast<double>(total_pkts) / seconds
              << std::endl;
  }

  rte_eth_stats_get(info.port_id, &stats);
  std::cerr << stats.imissed << ", " << stats.ierrors << ", " << stats.ipackets
            << ", " << stats.ibytes << std::endl;

  std::cerr << "done" << std::endl;
  close_port(info);
  osv::poweroff();
}
