#pragma once

// MiniDPDK shim for DPDK's <rte_io.h>. MMIO accessors and I/O memory barriers
// at global scope, mirroring DPDK's x86 mapping. Relaxed accessors are plain
// volatile loads/stores (no ordering, just the device access); the non-relaxed
// accessors add the matching I/O barrier. On x86 (TSO) hardware only reorders
// StoreLoad, so the read/write barriers are compiler barriers (OSv's barrier())
// and the full barrier is an mfence (processor::mfence()).

#include <cstdint>

#include <processor.hh>

#include <osv/barrier.hh>

// --- I/O memory barriers ---

inline void rte_io_mb() { processor::mfence(); }
inline void rte_io_wmb() { barrier(); }
inline void rte_io_rmb() { barrier(); }

// --- Relaxed MMIO accessors ---

inline uint8_t rte_read8_relaxed(const volatile void *addr) {
  return *reinterpret_cast<const volatile uint8_t *>(addr);
}
inline uint16_t rte_read16_relaxed(const volatile void *addr) {
  return *reinterpret_cast<const volatile uint16_t *>(addr);
}
inline uint32_t rte_read32_relaxed(const volatile void *addr) {
  return *reinterpret_cast<const volatile uint32_t *>(addr);
}
inline uint64_t rte_read64_relaxed(const volatile void *addr) {
  return *reinterpret_cast<const volatile uint64_t *>(addr);
}
inline void rte_write8_relaxed(uint8_t value, volatile void *addr) {
  *reinterpret_cast<volatile uint8_t *>(addr) = value;
}
inline void rte_write16_relaxed(uint16_t value, volatile void *addr) {
  *reinterpret_cast<volatile uint16_t *>(addr) = value;
}
inline void rte_write32_relaxed(uint32_t value, volatile void *addr) {
  *reinterpret_cast<volatile uint32_t *>(addr) = value;
}
inline void rte_write64_relaxed(uint64_t value, volatile void *addr) {
  *reinterpret_cast<volatile uint64_t *>(addr) = value;
}

// --- Non-relaxed accessors: relaxed access plus the matching I/O barrier ---

inline uint8_t rte_read8(const volatile void *addr) {
  uint8_t v = rte_read8_relaxed(addr);
  rte_io_rmb();
  return v;
}
inline uint16_t rte_read16(const volatile void *addr) {
  uint16_t v = rte_read16_relaxed(addr);
  rte_io_rmb();
  return v;
}
inline uint32_t rte_read32(const volatile void *addr) {
  uint32_t v = rte_read32_relaxed(addr);
  rte_io_rmb();
  return v;
}
inline uint64_t rte_read64(const volatile void *addr) {
  uint64_t v = rte_read64_relaxed(addr);
  rte_io_rmb();
  return v;
}
inline void rte_write8(uint8_t value, volatile void *addr) {
  rte_io_wmb();
  rte_write8_relaxed(value, addr);
}
inline void rte_write16(uint16_t value, volatile void *addr) {
  rte_io_wmb();
  rte_write16_relaxed(value, addr);
}
inline void rte_write32(uint32_t value, volatile void *addr) {
  rte_io_wmb();
  rte_write32_relaxed(value, addr);
}
inline void rte_write64(uint64_t value, volatile void *addr) {
  rte_io_wmb();
  rte_write64_relaxed(value, addr);
}
