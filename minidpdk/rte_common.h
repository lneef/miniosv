#pragma once

// MiniDPDK shim for DPDK's <rte_common.h>. The arithmetic, pointer and
// power-of-2 helpers the ENA driver relies on, as idiomatic C++20 constexpr
// functions / templates. The RTE_* names keep DPDK's call syntax, so driver
// code is unchanged, while gaining type safety over the original macros.

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

inline void rte_memcpy(void *dst, const void *src, size_t n){
    std::memcpy(dst, src, n);
}

// Pack a struct to its natural (unpadded) layout.
#ifndef __rte_packed
#define __rte_packed __attribute__((__packed__))
#endif

// Mark a function/parameter as possibly unused.
#ifndef __rte_unused
#define __rte_unused __attribute__((__unused__))
#endif

// DPDK's runtime assert maps onto the C-library assert (per the port spec).
#ifndef RTE_ASSERT
#define RTE_ASSERT(exp) assert(exp)
#endif

// Always inline a function, even at -O0.
#ifndef __rte_always_inline
#define __rte_always_inline inline __attribute__((always_inline))
#endif

// Align a type/variable to a cache line.
#ifndef __rte_cache_aligned
#define __rte_cache_aligned __attribute__((__aligned__(64)))
#endif

// Recover the enclosing struct from a pointer to one of its members.
#ifndef container_of
#define container_of(ptr, type, member)                                        \
  __extension__({                                                              \
    const __typeof__(((type *)0)->member) *_mptr = (ptr);                      \
    (type *)((char *)_mptr - offsetof(type, member));                          \
  })
#endif


// Pack a struct to its natural (unpadded) layout.
#ifndef __rte_packed
#define __rte_packed __attribute__((__packed__))
#endif

// Pointer advanced by a byte offset. Not constexpr: the cast crosses the
// integer/pointer boundary. Returns void* to mirror DPDK's macro.
inline void *RTE_PTR_ADD(const void *ptr, std::size_t x) {
  return const_cast<char *>(static_cast<const char *>(ptr)) + x;
}

// val rounded down to a multiple of align (a power of two). The result keeps
// val's type, matching DPDK's typeof()-based macro.
template <std::integral T, std::integral U>
constexpr T RTE_ALIGN_FLOOR(T val, U align) {
  return static_cast<T>(val & ~(static_cast<T>(align) - 1));
}

template <typename A, typename B>
constexpr std::common_type_t<A, B> RTE_MIN(A a, B b) {
  return a < b ? a : b;
}

template <typename A, typename B>
constexpr std::common_type_t<A, B> RTE_MAX(A a, B b) {
  return a > b ? a : b;
}

// Number of elements in a C array.
template <typename T, std::size_t N>
constexpr std::size_t RTE_DIM(const T (&)[N]) noexcept {
  return N;
}

// Mark a value as deliberately unused.
template <typename T> constexpr void RTE_SET_USED(const T &) noexcept {}

constexpr int rte_is_power_of_2(std::uint32_t n) { return n && !(n & (n - 1)); }

// Fill every bit below the most-significant set bit of x.
constexpr std::uint32_t rte_combine32ms1b(std::uint32_t x) {
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return x;
}

// Round x down to the previous power of two.
constexpr std::uint32_t rte_align32prevpow2(std::uint32_t x) {
  x = rte_combine32ms1b(x);
  return x - (x >> 1);
}
