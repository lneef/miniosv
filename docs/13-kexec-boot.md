# Booting miniosv from Linux via kexec (aarch64)

miniosv can now be started by a running Linux with `kexec`, in addition to
being loaded by UEFI firmware. Verified under QEMU TCG (`virt`, GICv3) and on
AWS Graviton (c7g.4xlarge, kexec'ed from Amazon Linux 2023's 6.18 kernel),
with both `kexec_file_load()` (`kexec -s`) and kexec-tools' `kexec_load()`
(`kexec -c`).

## The image: `loader.kexec`

`make arch=aarch64` additionally produces `build/<mode>/loader.kexec`, an arm64
Linux "Image" (`boot/kexec/image.S`, `image.ld`): the 64-byte Image header, a
`boot_info` slot, and the kernel ELF at offset `0x10000`, used in place. With
`text_offset = 0x80000`, Linux places the ELF header at `base + 0x90000` and
`text_start` at `base + 0xa0000` for a 2 MiB-aligned base - exactly the
geometry `arch/aarch64/boot.S` already assumes for a UEFI-loaded kernel, so no
copy is needed. `image_size` covers `.bss` (computed from `.edata` by the
Makefile). The trampoline fills `magic`, `kernel_phys_base` and the DTB address
into `boot_info` and jumps to `uefi_entry`.

## Changes in the kernel

- `include/osv/boot-info.hh`: `boot_info_magic_kexec` and a `dtb_addr` field
  (appended; the offsets `boot.S` reads are unchanged).
- `arch/aarch64/kexec-boot.cc` (new): runs first thing in
  `arch_init_early_console()` when the magic says kexec. A minimal FDT walker
  reads `/chosen`: `bootargs` becomes the command line; the UEFI memory map and
  system table that Linux's EFI stub preserved there give the memory map
  (classified like the UEFI stub does, minus the DTB, `/memreserve/` entries,
  the initrd, Linux's EFI memreserve chain - where its GICv3 LPI tables live -
  and `linux,usable-memory-range`), and the EFI configuration tables give the
  ACPI RSDP, hence the SPCR console UART. No wall clock after a kexec.
- `include/osv/boot-acpi.hh` (new): the SPCR lookup, shared with the UEFI stub
  (`boot/uefi/stub.cc` now uses it instead of its own copy).
- `arch/aarch64/boot.S`: `init_cpu` resets `TCR2_EL1`/`SCTLR2_EL1` when the CPU
  has them. Linux enables FEAT_S1PIE permission indirection there (on QEMU
  `-cpu max` and recent cores); firmware never does, and with it on the MMU
  faulted on perfectly good page tables. The boot identity map now covers
  512 GiB (kexec_file puts the DTB at the top of RAM).
- `arch/aarch64/gic-v3.cc/.hh`: before programming anything, quiesce what the
  previous kernel left running - `GITS_CTLR` to quiescent, `GICR_CTLR.EnableLPIs`
  cleared (IMPLEMENTATION DEFINED whether that works; the kernel aborts with a
  message if not - adopting Linux's LPI tables is not implemented), pending
  SPIs/PPIs/SGIs cleared.
- `arch/aarch64/arm-clock.cc`: also disarm the EL1 physical timer.
- `core/trace.cc`: removed a write-only global that clang 23 rejects under
  `-Werror` (unrelated, needed to build).

## Limits

The kexec'ing Linux must run at EL1 (QEMU `virt` without `virtualization=on`;
an EL2 host hands over at EL2, which `boot.S` rejects), must itself have booted
via UEFI (the DTB needs the `linux,uefi-*` properties), and should not use
`kho=on` for the miniosv kexec. miniosv's "Booted up in" time is measured from
the generic timer's zero, which kexec does not reset.

## Reproduction

Under QEMU (TCG; needs `qemu-system-aarch64`, the `qemu-efi-aarch64` firmware,
clang/lld, and `aarch64-linux-gnu-gcc` for the static busybox/kexec-tools):

```bash
make arch=aarch64 app=scripts/linux-kexec/app       # miniosv that prints KEXEC-TEST: PASS and powers off
scripts/linux-kexec/build.sh --payload build/release.aarch64/loader.kexec
scripts/linux-kexec/run.py --test miniosv-file        # kexec_file_load() into miniosv
scripts/linux-kexec/run.py --test miniosv-load        # kexec_load() (kexec-tools) into miniosv
scripts/linux-kexec/run.py --test file,load,kho,kdump # Linux-only kexec self-test
```

Each prints the serial console and exits 0 on `KEXEC-TEST: PASS`. Without
`--test`, `run.py` boots the harness to a shell where the kexec commands can
be run by hand (`kexec -s -l /boot/payload/loader.kexec && kexec -e`).

On AWS (needs terraform, the aws CLI with credentials, and
`ec2-instance-connect:SendSerialConsoleSSHPublicKey` for the live console):

```bash
AWS_REGION=eu-central-1 INSTANCE_TYPE=c7g.4xlarge \
    scripts/linux-kexec/aws/test.sh all --steps miniosv-file
```

This creates the instance, kexecs the harness from the Amazon Linux kernel,
watches the serial console, saves it to `build/linux-kexec/aws-console-<id>.txt`
and destroys everything again.

## Test harness: `scripts/linux-kexec/`

A minimal aarch64 Linux (6.18 LTS, config derived from the Amazon Linux 2023
Graviton kernel, only the hardware miniosv supports, busybox + kexec-tools in
a built-in initramfs) that boots under QEMU + UEFI and chains kexecs:
`run.py --test file,load,kho,kdump` self-tests Linux's kexec features,
`run.py --test miniosv-file|miniosv-load` kexecs into `loader.kexec`.
`aws/` holds a Terraform setup (`test.sh all`) that does the same on a
Graviton instance and watches the EC2 serial console. See its README.
