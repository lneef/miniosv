#pragma once

// MiniDPDK shim for DPDK's <rte_log.h>. Logging is routed through OSv's logger
// (logger::wrt): every rte_log maps its DPDK level to an OSv logger_severity via
// rte_log_severity() and always logs at that severity, so OSv's per-severity
// filtering applies on every call. Only the subset the ENA driver references is
// provided (RTE_LOGTYPE_* themselves come from the driver's ena_logs.h).

#include <cstdarg>
#include <cstdint>

#include <osv/debug.hh>

// Log levels — DPDK's canonical numeric values (lower = more severe), only the
// ones ENA references (EMERG is unused).
#define RTE_LOG_ALERT   2
#define RTE_LOG_CRIT    3
#define RTE_LOG_ERR     4
#define RTE_LOG_WARNING 5
#define RTE_LOG_NOTICE  6
#define RTE_LOG_INFO    7
#define RTE_LOG_DEBUG   8

// Tag used for all DPDK shim logging; OSv filters severities per tag.
#define RTE_LOG_OSV_TAG "osv"

// Map a DPDK log level onto OSv's logger_severity (osv/debug.h). DPDK has finer
// levels than OSv, so several collapse: NOTICE/INFO -> info, and everything at
// least as severe as ERR (ERR/CRIT/ALERT/EMERG) -> error.
inline logger_severity rte_log_severity(uint32_t level) {
    switch (level) {
    case RTE_LOG_DEBUG:   return logger_debug;
    case RTE_LOG_INFO:
    case RTE_LOG_NOTICE:  return logger_info;
    case RTE_LOG_WARNING: return logger_warn;
    default:              return logger_error;
    }
}

// Emit a message through OSv's logger at the mapped severity. logtype is unused
// (OSv filters by tag+severity). No newline is added here — OSv's logger appends
// one — so the RTE_LOG_LINE* macros do not add one either.
__attribute__((format(printf, 3, 4)))
inline int rte_log(uint32_t level, uint32_t logtype, const char *format, ...) {
    (void)logtype;
    va_list ap;
    va_start(ap, format);
    logger::instance()->wrt(RTE_LOG_OSV_TAG, rte_log_severity(level), format, ap);
    va_end(ap);
    return 0;
}

// Log one line: <prefix><fmt>. `prefix` and `fmt` must be string literals (they
// are concatenated); `args` are the prefix's arguments and the trailing varargs
// are fmt's. The `, ##__VA_ARGS__` extension drops the comma when fmt takes no
// arguments. The trailing newline is added by OSv's logger.
#define RTE_LOG_LINE_PREFIX(level, logtype, prefix, args, fmt, ...)	       \
	rte_log(RTE_LOG_ ## level, RTE_LOGTYPE_ ## logtype,		       \
		prefix fmt, args, ##__VA_ARGS__)

// "%.0s" consumes the empty prefix-arg and prints nothing.
#define RTE_LOG_LINE(level, logtype, ...)				       \
	RTE_LOG_LINE_PREFIX(level, logtype, "%.0s", "", __VA_ARGS__)

// Define a log-type id. OSv filters by tag+severity rather than a per-logtype
// level, so this just defines the global the driver declares extern in
// ena_logs.h; the registration level is kept for source compatibility.
#define RTE_LOG_REGISTER_SUFFIX(type, suffix, level) int type = RTE_LOG_ ## level
