#pragma once
#include <cstdint>

inline uint64_t rte_rand(void) {
  static uint64_t state = 0x9e3779b97f4a7c15ULL;
  state ^= state << 13;
  state ^= state >> 7;
  state ^= state << 17;
  return state;
}
