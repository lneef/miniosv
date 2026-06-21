/*
 * Copyright (C) 2023 Lukas Neef 
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <sys/cdefs.h>

#include "drivers/nic.hh"
#include "drivers/pci-device.hh"
#include "minidpdk/driver/probe.hh"

#include <osv/aligned_new.hh>
#include <osv/debug.hh>

namespace nic {

nic::nic(std::unique_ptr<minidpdk::pci_device> dev) : _dev(std::move(dev)) {}

nic::~nic() {
    minidpdk::remove_nic(*_dev);
}

void nic::dump_config(void) { _dev->dev->dump_config(); }

hw_driver *nic::probe(hw_device *dev) {
  try {
    if (auto pci_dev = dynamic_cast<pci::device *>(dev)) {
      pci_dev->dump_config();
      auto handle = std::make_unique<minidpdk::pci_device>(*pci_dev);
      if (minidpdk::probe_nics(*handle))
        return aligned_new<nic>(std::move(handle));
    }
  } catch (std::exception &e) {
    debugf("nic: exception on device construction: %s\n", e.what());
  }
  return nullptr;
}

} // namespace nic
