#pragma once

// MiniDPDK shim for DPDK's <rte_atomic.h>. Types and free functions at global
// scope, mirroring DPDK. The atomic counters are backed by std::atomic; the
// memory barriers are deliberately left unimplemented (see below).
//
// Only the subset the ENA driver references is provided.

#include <atomic>
#include <cstdint>

// DPDK keeps the counter in a struct member named `cnt`; we preserve that
// layout but make it a std::atomic. Relaxed ordering matches DPDK's plain
// `volatile cnt` accesses.
typedef struct { std::atomic<int32_t> cnt; } rte_atomic32_t;
typedef struct { std::atomic<int64_t> cnt; } rte_atomic64_t;

inline void rte_atomic32_set(rte_atomic32_t *v, int32_t n) {
    v->cnt.store(n, std::memory_order_relaxed);
}
inline int32_t rte_atomic32_read(const rte_atomic32_t *v) {
    return v->cnt.load(std::memory_order_relaxed);
}
inline void rte_atomic32_inc(rte_atomic32_t *v) {
    v->cnt.fetch_add(1, std::memory_order_relaxed);
}
inline void rte_atomic32_dec(rte_atomic32_t *v) {
    v->cnt.fetch_sub(1, std::memory_order_relaxed);
}

inline void rte_atomic64_init(rte_atomic64_t *v) {
    v->cnt.store(0, std::memory_order_relaxed);
}
inline int64_t rte_atomic64_read(const rte_atomic64_t *v) {
    return v->cnt.load(std::memory_order_relaxed);
}
inline void rte_atomic64_inc(rte_atomic64_t *v) {
    v->cnt.fetch_add(1, std::memory_order_relaxed);
}

// Memory barriers: OSv provides no full memory barrier and no I/O barrier, only
// a compiler barrier() and processor::lfence(). How to realise these on OSv is
// still undecided, so they are stubbed as deleted functions: the header still
// compiles, but any call site fails to compile until a real implementation is
// chosen.
// TODO: implement rte_mb / rte_wmb / rte_rmb.
void rte_mb()  = delete;
void rte_wmb() = delete;
void rte_rmb() = delete;
