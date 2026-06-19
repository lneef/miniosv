#pragma once

// MiniDPDK shim for DPDK's <rte_cycles.h>. Free functions at global scope,
// mirroring DPDK. `cycles` are TSC ticks read via OSv's processor::ticks();
// the frequency is derived once in internal/cycles.hh (OSv exposes no direct
// TSC frequency).

#include <cstdint>
#include <unistd.h>

#include <processor.hh>

#include <minidpdk/internal/cycles.hh>

// Compute and cache the timer frequency. Call once during startup, before any
// rte_get_timer_hz() call, like DPDK derives the TSC hz in rte_eal_init().
inline void rte_cycles_init() {
    minidpdk::internal::timer_hz = minidpdk::internal::tsc_hz();
}

// Current cycle counter (TSC). Matches the clock used by rte_timer ticks.
inline uint64_t rte_get_timer_cycles() {
    return static_cast<uint64_t>(processor::ticks());
}

// Timer frequency in Hz, as cached by rte_cycles_init(). A plain read of the
// global; zero if rte_cycles_init() has not run yet.
inline uint64_t rte_get_timer_hz() {
    return minidpdk::internal::timer_hz;
}

// DPDK TSC aliases — on this platform the timer clock *is* the TSC.
inline uint64_t rte_rdtsc()          { return rte_get_timer_cycles(); }
inline uint64_t rte_get_tsc_cycles() { return rte_get_timer_cycles(); }
inline uint64_t rte_get_tsc_hz()     { return rte_get_timer_hz(); }

// Busy-wait for at least `us` microseconds by spinning on the TSC. Needs
// rte_cycles_init() to have run; otherwise the cycle count is zero and this
// returns immediately.
inline void rte_delay_us_block(unsigned int us) {
    const uint64_t start = rte_get_timer_cycles();
    const uint64_t cycles = static_cast<uint64_t>(us) * rte_get_timer_hz() / 1000000;
    while (rte_get_timer_cycles() - start < cycles)
        ;
}

// Sleeping delay: yields the CPU via OSv's usleep instead of busy-waiting.
inline void rte_delay_us_sleep(unsigned int us) {
    usleep(us);
}

// Generic microsecond delay. DPDK's default points this at the blocking
// variant (callback registration is not modelled), so we do the same.
inline void rte_delay_us(unsigned int us) {
    rte_delay_us_block(us);
}
