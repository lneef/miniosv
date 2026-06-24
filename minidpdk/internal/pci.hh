#pragma once

#include "minidpdk/rte_lcore.h"
#include "osv/sched.hh"
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <drivers/pci-device.hh>
#include <osv/msi.hh>

struct rte_mem_resource {
    uint64_t phys_addr;   
    uint64_t len;         
    void *addr; 
};

struct rte_pci_driver;
struct rte_eth_dev;

namespace minidpdk {
inline constexpr size_t kMaxPCIBarCount = 8;

using irq_handler = void (*)(void *);
struct intr_config {
    irq_handler handler = nullptr;
    void *arg = nullptr;
};

struct intr_handle {
    const unsigned num_entries = 0;        
    pci::function *dev = nullptr;
    std::vector<msix_vector*> vectors;     

    intr_handle() = default;
    explicit intr_handle(pci::function *dev)
        : num_entries(dev->msix_get_num_entries()), dev(dev) {}

    int alloc(unsigned driver_max) {
        if (driver_max > num_entries)
            return -ENOSPC;
        vectors.assign(driver_max, nullptr);
        return 0;
    }

    size_t size() const { return vectors.size(); }
    msix_vector *get(unsigned idx) { return vectors[idx]; }
    void enable() { dev->msix_enable(); }

    int assign(unsigned idx, void (*isr)(void *), void *arg) {
        if (idx >= vectors.size())
            return -ENOSPC;
        if (vectors[idx] != nullptr)
            return -EEXIST;
        interrupt_manager im(dev);

        // device limitations checked above
        auto vs = im.request_vectors(1);
        if (vs.empty())
            return -ENOSPC;
        msix_vector *v = vs[0];
        im.assign_isr(v, [isr, arg] { isr(arg); });
        if (!im.setup_entry(idx, v)) {
            delete v;
            return -EIO;
        }
        vectors[idx] = v;
        return 0;
    }

    void set_affinity(unsigned idx, uint16_t req_cpu) {
        vectors[idx]->set_affinity(sched::cpus[req_cpu % rte_lcore_count()]);
    }

    void unmask(unsigned idx) { vectors[idx]->msix_unmask_entries(); }

    void free(unsigned idx) {
        if (idx < vectors.size() && vectors[idx]) {
            vectors[idx]->msix_mask_entries();
            delete vectors[idx];
            vectors[idx] = nullptr;
        }
    }
};

struct pci_device {
public:
    pci_device() = default;
    explicit pci_device(pci::device &dev)
        : intr_handle{ &dev },
          dev(&dev) {
        assert(dev.is_msix()); 
    }

    void msix_enable() { dev->msix_enable(); }
    void msix_disable() { dev->msix_disable(); }

    // Map the device's MMIO BARs and record each region in mem_resource. OSv
    // numbers BARs 1-based (see pci::device::parse_pci_config); DPDK indexes
    // mem_resource by 0-based BAR number, hence idx - 1.
    void map_resources() {
        for (auto [idx, bar] : *dev) {
            rte_mem_resource &res = mem_resource[idx - 1];
            res.phys_addr = bar->get_addr64();
            res.len = bar->get_size();
            if (bar->is_mmio()) {
                if (!bar->is_mapped())
                    bar->map();
                res.addr = const_cast<void *>(bar->get_mmio());
            }
        }
    }

    void unmap_resources() {
        for (auto [idx, bar] : *dev) {
            if (bar->is_mmio() && bar->is_mapped())
                bar->unmap();
            mem_resource[idx - 1] = {};
        }
    }

    minidpdk::intr_handle intr_handle{};
    pci::device *dev = nullptr;
    rte_pci_driver* drv;
    rte_eth_dev *eth_dev;
    rte_mem_resource mem_resource[kMaxPCIBarCount]{};  
};

} // namespace minidpdk
