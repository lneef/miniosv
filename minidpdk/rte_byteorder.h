#pragma once
/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

// MiniDPDK shim for DPDK's <rte_byteorder.h>. Provides the endian macros, the
// rte_be*/rte_le* width typedefs and the host<->big-endian converters the
// rte_ip.h checksum helpers (and the ENA driver) rely on. These are plain
// compiler builtins with no OSv dependency, mirroring rte_branch_prediction.h.

#include <cstdint>

#define RTE_LITTLE_ENDIAN 1234
#define RTE_BIG_ENDIAN    4321

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define RTE_BYTE_ORDER RTE_BIG_ENDIAN
#else
#define RTE_BYTE_ORDER RTE_LITTLE_ENDIAN
#endif

using rte_be16_t = uint16_t;
using rte_be32_t = uint32_t;
using rte_be64_t = uint64_t;
using rte_le16_t = uint16_t;
using rte_le32_t = uint32_t;
using rte_le64_t = uint64_t;

#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
inline rte_be16_t rte_cpu_to_be_16(uint16_t x) { return x; }
inline rte_be32_t rte_cpu_to_be_32(uint32_t x) { return x; }
inline uint16_t rte_be_to_cpu_16(rte_be16_t x) { return x; }
inline uint32_t rte_be_to_cpu_32(rte_be32_t x) { return x; }
#else
inline rte_be16_t rte_cpu_to_be_16(uint16_t x) { return __builtin_bswap16(x); }
inline rte_be32_t rte_cpu_to_be_32(uint32_t x) { return __builtin_bswap32(x); }
inline uint16_t rte_be_to_cpu_16(rte_be16_t x) { return __builtin_bswap16(x); }
inline uint32_t rte_be_to_cpu_32(rte_be32_t x) { return __builtin_bswap32(x); }
#endif
