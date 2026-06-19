#pragma once

// MiniDPDK shim for DPDK's <rte_alarm.h>. The public surface mirrors DPDK (free
// functions and the callback typedef at global scope).
//
// rte_alarm is a one-shot, fire-from-timer-context callback. A common pattern —
// and the only way the ENA driver uses it — is a self-rearming alarm that polls
// the control path every interval. We don't want a control-path poll loop for
// now, so we deliberately leave this unimplemented: the calls always fail, ENA
// never enters polling mode and falls back to its interrupt-driven control path.
// The functions exist only so the driver compiles and links.

#include <cstdint>

// Callback invoked when an alarm fires, with the user-supplied argument.
typedef void (*rte_eal_alarm_callback)(void *arg);

// Schedule `cb_fn(cb_arg)` to fire once after `us` microseconds.
// Stubbed to always fail (-1): see the note above on avoiding a control-path
// poll loop for now.
inline int rte_eal_alarm_set(uint64_t us, rte_eal_alarm_callback cb_fn,
                             void *cb_arg) {
    (void)us;
    (void)cb_fn;
    (void)cb_arg;
    return -1;
}

// Cancel pending alarms matching (cb_fn, cb_arg). Nothing is ever armed, so
// there is nothing to cancel.
inline int rte_eal_alarm_cancel(rte_eal_alarm_callback cb_fn, void *cb_arg) {
    (void)cb_fn;
    (void)cb_arg;
    return -1;
}
