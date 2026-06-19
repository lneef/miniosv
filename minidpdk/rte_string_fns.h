#pragma once

// MiniDPDK shim for DPDK's <rte_string_fns.h>. Only rte_strscpy — the one
// function the ENA driver references — is provided, implemented on top of OSv's
// strlcpy.

#include <errno.h>
#include <string.h>
#include <sys/types.h>   // ssize_t

// Copy src into dst (buffer size dsize), always NUL-terminating when dsize > 0.
// Returns the number of bytes written excluding the NUL on success, or -E2BIG if
// src did not fit (dst is still NUL-terminated). Matches DPDK's rte_strscpy.
//
// OSv's strlcpy returns strlen(src): if that is < dsize the whole string fit,
// otherwise the copy was truncated.
inline ssize_t rte_strscpy(char *dst, const char *src, size_t dsize) {
    size_t src_len = strlcpy(dst, src, dsize);
    if (src_len < dsize) {
        return static_cast<ssize_t>(src_len);
    }
    return -E2BIG;
}
