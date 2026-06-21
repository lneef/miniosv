#pragma once

// MiniDPDK shim for DPDK's <rte_bitops.h>. The single-bit mask helpers the ENA
// driver's BIT()/BIT64() macros expand to, as C++20 constexpr functions. They
// keep the RTE_BIT*() call syntax, so values stay usable in constant
// expressions (enum/case labels) while dropping the macro footguns.

#include <cstdint>

constexpr std::uint64_t RTE_BIT64(unsigned nr) { return std::uint64_t{1} << nr; }
constexpr std::uint32_t RTE_BIT32(unsigned nr) { return std::uint32_t{1} << nr; }
