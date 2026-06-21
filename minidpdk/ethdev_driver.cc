#include <cerrno>
#include <cstdlib>

#include <minidpdk/ethdev_driver.h>
#include <minidpdk/rte_malloc.h>

// Definition of the global ethdev port table declared in <ethdev_driver.h>.
rte_eth_dev::store_t rte_eth_dev::store{};

// Definition of the global PCI driver registry declared in <ethdev_driver.h>.
rte_pci_driver::registry_t rte_pci_driver::registry{};

int rte_eth_dev_pci_generic_probe(minidpdk::pci_device *pci_dev,
                                  size_t private_data_size,
                                  eth_dev_pci_callback_t dev_init) {
  // rte_eth_dev carries a non-trivial intrusive hook, so construct it in place
  // on cacheline-aligned storage; the two POD blocks come from zmalloc.
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

  // Private data must exist before dev_init, which dereferences it.
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

  rte_eth_dev::store.push_back(*eth_dev);
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
  rte_free(eth_dev->data->dev_private);
  rte_free(eth_dev->data);
  eth_dev->~rte_eth_dev(); 
  std::free(eth_dev);
  return 0;
}
