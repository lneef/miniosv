#pragma once

#include <cstdint>

#include <processor.hh>

#include <drivers/clock.hh>

namespace minidpdk::internal {

// Cached timer frequency in Hz. Set once by rte_cycles_init(); read by
// rte_get_timer_hz(). Zero until initialised.
inline uint64_t timer_hz = 0;

// TSC frequency in Hz. OSv exposes no direct frequency, so derive it from the
// clock's tick->nanosecond conversion: for T ticks the clock reports N ns, so
// hz = T * 1e9 / N. A large T minimises rounding from the conversion's shift.
inline uint64_t tsc_hz() {
    constexpr uint64_t nano_per_sec = 1000000000ull;
    constexpr uint64_t ref_ticks = 1ull << 32;
    uint64_t nanos = clock::get()->processor_to_nano(ref_ticks);
    return static_cast<uint64_t>(
        (static_cast<__uint128_t>(ref_ticks) * nano_per_sec) / nanos);
}

} // namespace minidpdk::internal
