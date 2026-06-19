#pragma once

// MiniDPDK shim for DPDK's <rte_timer.h>. The public surface mirrors DPDK (free
// functions and types at global scope). As in DPDK, `ticks` are TSC cycles
// (cf. rte_get_timer_cycles) and `tim_lcore` selects the cpu that owns the timer.
//
// TODO: implement. This is currently an empty shim -- timers are never armed and
// callbacks never fire. The calls are accepted and report success so drivers
// (ENA) compile and run, but any periodic work driven by rte_timer (e.g. ENA's
// watchdog) is effectively disabled until this is backed by a real timer.

#include <cstdint>

// DPDK timer type: one-shot or auto-reloading. Values match <rte_timer.h>.
enum rte_timer_type { SINGLE, PERIODICAL };

struct rte_timer;
using rte_timer_cb_t = void (*)(rte_timer *, void *);

// Opaque DPDK timer handle. Fields will be added when this is implemented.
struct rte_timer {};

inline int rte_timer_subsystem_init() {
    return 0;
}

inline void rte_timer_init(rte_timer *tim) {
    (void)tim;
}

inline int rte_timer_reset(rte_timer *tim, uint64_t ticks,
                           rte_timer_type type, unsigned tim_lcore,
                           rte_timer_cb_t fct, void *arg) {
    (void)tim;
    (void)ticks;
    (void)type;
    (void)tim_lcore;
    (void)fct;
    (void)arg;
    return 0;
}

inline void rte_timer_reset_sync(rte_timer *tim, uint64_t ticks,
                                 rte_timer_type type, unsigned tim_lcore,
                                 rte_timer_cb_t fct, void *arg) {
    (void)tim;
    (void)ticks;
    (void)type;
    (void)tim_lcore;
    (void)fct;
    (void)arg;
}

inline int rte_timer_stop(rte_timer *tim) {
    (void)tim;
    return 0;
}

inline void rte_timer_stop_sync(rte_timer *tim) {
    (void)tim;
}

inline int rte_timer_pending(rte_timer *tim) {
    (void)tim;
    return 0;
}

inline void rte_timer_manage() {}
