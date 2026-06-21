/*
 * Copyright (C) 2026 Lukas Neef
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef NIC_HH
#define NIC_HH

#include <memory>

#include "drivers/driver.hh"
#include "minidpdk/internal/pci.hh"

namespace nic {
class nic : public hw_driver {
public:
  explicit nic(std::unique_ptr<minidpdk::pci_device> dev);
  virtual ~nic();

  virtual std::string get_name() const override { return "nic"; }

  virtual void dump_config() override;

  static hw_driver *probe(hw_device *dev);

private:
  std::unique_ptr<minidpdk::pci_device> _dev;
};

} // namespace nic

#endif
