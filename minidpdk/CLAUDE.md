# MiniDPDK

This directory is the `MiniDPDK` shim layer: a DPDK-compatible API implemented
solely on top of OSv features.

## Structure
- `minidpdk/*` — the shim layer. Public DPDK-facing API surface lives here.
- `minidpdk/internal/` — the actual implementation structure backing the shim.
- `minidpdk/driver/` — NIC driver implementations.

## Guidelines
- Everything under `minidpdk/` is shim code. Keep the public surface thin and
  forward to `internal/` for the real work.
- Build on OSv whenever possible; do not reimplement features OSv already
  provides (see the repo-root `CLAUDE.md`).

## Notes
- ignore socket information/arguments for now
    - add function arguments
    - mark them as unused
