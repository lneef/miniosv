This is a stripped down version of the OSv-Unikernel. 

## Objective
We want to create a DPDK shim layer `MiniDPDK` relying solely on OSv features.

## Structure
- `drivers/` contains hardware drivers
- `core/` core operating system library (scheduler, memmory management, semaphores)
- `app/` osv app running inside the unikernel
- `arch/` processor specific structures and functionality(paging, setup, control register, firmware)
    -`arch/x64` x64 specific functionality
    - `arch/aarch64` aarch64 specific functionality
    - `arch/common` common functionality (mainly interface definition)
- `include/` structure and function definitions
    - `include/api` syscall/operating system api with POSIX-API functions like(mmap)
    - `include/lockfree` lockfree data structures and sync primitives
    - `include/osv` osv's core api definitions
## Guidelines
- Write idiomatic C++20 code
    - use concepts instead of sth. like SFINAE or plain templates
    - aggressively use constexpr and consteval
    - std::span instead of ptr to ptr
- Use OSv whenever possible
    - If you implement an Operating System/Runtime Feature ensure that it is not already implemented by OSv (e.g. OSv already implements `wrmsr` for x64 so it must not be implemented via inline asm)

### 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

### 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that isn't required.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

### 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.

## Build & run

```bash
make                               # build x86-64 kernel (app/app.cc)
./scripts/run.py                   # boot under QEMU/KVM
```

The default app (`app/app.cc`) is a conformance gate that prints a `PASS`/`FAIL` line per check.
Build the larger test suite with `make app=tests`.

## Verification
- Always verify compilation of osv. Do it always after finishing a task
