#pragma once

// MiniDPDK shim for DPDK's <rte_spinlock.h>. Backed by OSv's non-preemption
// spinlock (np_spinlock) from <osv/spinlock.h>: like DPDK's plain spinlock it
// does not toggle preemption on lock/unlock. Only the subset the ENA driver
// references is provided.

#include <osv/spinlock.h>

typedef np_spinlock_t rte_spinlock_t;

inline void rte_spinlock_init(rte_spinlock_t *sl) {
    np_spinlock_init(sl);
}
inline void rte_spinlock_lock(rte_spinlock_t *sl) {
    np_spin_lock(sl);
}
inline void rte_spinlock_unlock(rte_spinlock_t *sl) {
    np_spin_unlock(sl);
}
