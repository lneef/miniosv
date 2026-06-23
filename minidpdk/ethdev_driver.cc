#include <algorithm>
#include <cerrno>
#include <cstdlib>

#include <minidpdk/ethdev_driver.h>
#include <minidpdk/rte_malloc.h>

constinit rte_eth_dev *rte_eth_dev::store[RTE_MAX_ETHPORTS]{};
constinit rte_pci_driver::registry_t rte_pci_driver::registry{};

int rte_eth_dev_pci_generic_probe(minidpdk::pci_device *pci_dev,
                                  size_t private_data_size,
                                  eth_dev_pci_callback_t dev_init) {
  // Claim the first free slot in the global port table; its index is the port id.
  auto slot = std::ranges::find(rte_eth_dev::store, nullptr);
  if (slot == std::ranges::end(rte_eth_dev::store))
    return -ENOSPC;
  const auto port_id =
      static_cast<uint16_t>(slot - std::ranges::begin(rte_eth_dev::store));

  void *mem = std::aligned_alloc(RTE_CACHE_LINE_SIZE, sizeof(rte_eth_dev));
  if (!mem)
    return -ENOMEM;
  auto *eth_dev = new (mem) rte_eth_dev{};
  eth_dev->pci_dev = pci_dev;

  eth_dev->data = static_cast<rte_eth_dev_data *>(rte_zmalloc_socket(
      "ethdev data", sizeof(rte_eth_dev_data), RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY));
  if (!eth_dev->data) {
    eth_dev->~rte_eth_dev();
    std::free(eth_dev);
    return -ENOMEM;
  }

  eth_dev->data->dev_private = rte_zmalloc_socket(
      "ethdev private", private_data_size, RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
  if (!eth_dev->data->dev_private) {
    rte_free(eth_dev->data);
    eth_dev->~rte_eth_dev();
    std::free(eth_dev);
    return -ENOMEM;
  }

  // Map the device BARs so dev_init can reach the register space.
  assert(pci_dev->drv->drv_flags & RTE_PCI_DRV_NEED_MAPPING);
  pci_dev->map_resources();

  int ret = dev_init(eth_dev);
  if (ret) {
    pci_dev->unmap_resources();
    rte_free(eth_dev->data->dev_private);
    rte_free(eth_dev->data);
    eth_dev->~rte_eth_dev();
    std::free(eth_dev);
    return ret;
  }

  // Publish in the port table so the port-id-indexed rte_eth_* wrappers resolve.
  eth_dev->data->port_id = port_id;
  *slot = eth_dev;
  pci_dev->eth_dev = eth_dev;
  return 0;
}

int rte_eth_dev_pci_generic_remove(minidpdk::pci_device *pci_dev,
                                   eth_dev_pci_callback_t dev_uninit) {
  auto* eth_dev = pci_dev->eth_dev;
  assert(eth_dev);
  int ret = dev_uninit ? dev_uninit(eth_dev) : 0;
  if (ret)
    return ret;

  pci_dev->unmap_resources();
  rte_eth_dev::store[eth_dev->data->port_id] = nullptr;
  rte_free(eth_dev->data->dev_private);
  rte_free(eth_dev->data);
  eth_dev->~rte_eth_dev();
  std::free(eth_dev);
  return 0;
}

int rte_eth_dev_callback_process(struct rte_eth_dev *dev,
		enum rte_eth_event_type event, void *ret_param){
    (void)dev;
    (void)event;
    (void)ret_param;
    return 0;
}
