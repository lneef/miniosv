#pragma once

#include <minidpdk/internal/pci.hh>

namespace minidpdk {

bool probe_nics(minidpdk::pci_device &pdev);

void remove_nic(minidpdk::pci_device& pdev);

} // namespace minidpdk
