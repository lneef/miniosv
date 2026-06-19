#pragma once

// MiniDPDK shim for DPDK's <rte_atomic.h>. Types and free functions at global
// scope, mirroring DPDK. The atomic counters are backed by std::atomic; the
// memory barriers are deliberately left unimplemented (see below).
//
// Only the subset the ENA driver references is provided.

#include <atomic>
#include <cstdint>

#include <processor.hh>

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

// Memory barriers: x86 hardware fences, mirroring DPDK's x86 mapping of
// rte_mb/rte_wmb/rte_rmb onto mfence/sfence/lfence. The fence helpers live in
// OSv's processor namespace (processor::lfence() was already there; sfence/mfence
// follow the same idiom).
inline void rte_mb()  { processor::mfence(); }
inline void rte_wmb() { processor::sfence(); }
inline void rte_rmb() { processor::lfence(); }
