#include <cassert>
#include <cstddef>
#include <cstdlib>

#include <minidpdk/driver/probe.hh>
#include <minidpdk/ethdev_driver.h>

namespace minidpdk {

bool probe_nics(minidpdk::pci_device &pdev) {
    const uint16_t vendor = pdev.dev->get_vendor_id();
    const uint16_t device = pdev.dev->get_device_id();
    for (auto &drv : rte_pci_driver::registry) {
        for (const rte_pci_id &id : drv.id_table) {
            if (id.vendor_id == vendor &&
                (id.device_id == device || id.device_id == PCI_ANY_ID)){
                pdev.drv = &drv;
                drv.probe(&drv, &pdev);
                return true;
            }
        }
    }
    return false;
}

void remove_nic(minidpdk::pci_device &pdev){
    assert(pdev.drv);
    pdev.drv->remove(&pdev);
}
} // namespace minidpdk
