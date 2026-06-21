#pragma once

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

    struct vec_table {
        pci::function *dev = nullptr;
        std::vector<msix_vector*> vectors;

        vec_table() = default;
        explicit vec_table(pci::function *dev) : dev(dev) {}

        size_t size() const { return vectors.size(); }
        void resize(unsigned n) { vectors.resize(n, nullptr); }

        msix_vector *get(unsigned idx) {
            if (vectors[idx] == nullptr)
                vectors[idx] = new msix_vector(dev);
            return vectors[idx];
        }

        void assign(unsigned idx, void (*fn)(void *), void *arg) {
            get(idx)->set_handler([fn, arg] { fn(arg); });
        }

        void unmask() {
            for (auto *v : vectors)
                if (v != nullptr)
                    v->msix_unmask_entries();
        }

        void free() {
            for (auto *v : vectors)
                delete v;
            vectors.clear();
        }
    };

    vec_table control;   
    vec_table datapath;  

    intr_handle() = default;
    intr_handle(pci::function *dev)
        : num_entries(dev->msix_get_num_entries()), dev(dev),
          control(dev), datapath(dev) {}

    int ctrl_alloc(unsigned n) {
        if (vec_count() + n > num_entries)
            return -ENOSPC;
        control.resize(n);
        return 0;
    }

    int dp_alloc(unsigned n) {
        if (vec_count() + n > num_entries)
            return -ENOSPC;
        datapath.resize(n);
        return 0;
    }

    size_t vec_count() const{
        return datapath.size() + control.size();
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
