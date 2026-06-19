#pragma once

// MiniDPDK shim for DPDK's <rte_branch_prediction.h>. Provides the branch-hint
// macros the ENA driver relies on. These are plain compiler builtins with no
// OSv dependency, so the shim is identical to upstream DPDK. The #ifndef guards
// keep us from clashing with any other header that already defines them.

#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif
