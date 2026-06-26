# MiniDPDK

A minimal, DPDK-compatible shim layer implemented **solely on top of OSv**.

## Intent

MiniDPDK lets unmodified DPDK driver code (currently the Amazon ENA PMD) build
and run inside the OSv unikernel without pulling in DPDK's EAL. It re-exposes the
slice of DPDK's public API the driver touches (`rte_*` headers, `ethdev_driver.h`)
and backs it with OSv primitives — scheduler threads, the memory allocator,
timers, MMIO, logging — rather than DPDK's own runtime. The API surface is kept
thin: public shims forward to the real implementation in `internal/`.

## Based on

- **DPDK 26.07.0-rc1** — the revision the `rte_*` shims and the ENA driver were
  ported from.
- **ENA PMD 2.14.0** (`driver/ena/`) — the upstream `drivers/net/ena` tree,
  adapted to compile as C++ against the shims (see `driver/ena/port-ena.md`).

## Layout

- `*.h` / `*.cc` — the shim layer; the public DPDK-facing API.
- `internal/` — the implementation backing the shim (mem pool, page store,
  timers, PCI wrapper).
- `driver/` — NIC driver ports (`ena/`).

## Licensing

MiniDPDK is BSD-3-Clause, consistent with both OSv and DPDK.

Files that copy copyrightable DPDK source (struct layouts, flag-value tables,
checksum algorithms) retain their upstream SPDX/copyright headers — the ENA tree
(Amazon) and the derived `rte_*` shims (Intel / 6WIND / Regents of the
University of California). Files that only reimplement DPDK's API on OSv (e.g.
`rte_log.h`, `rte_bitops.h`, everything in `internal/`) are original work and
carry no upstream header. Copyright lines were verified against DPDK 26.07.0-rc1.

The full BSD 3-Clause license text the `SPDX-License-Identifier` tags refer to is
in [`LICENSE`](./LICENSE).
