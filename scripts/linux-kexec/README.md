# linux-kexec: a Linux that kexecs into miniosv

A minimal aarch64 Linux image that boots under QEMU (TCG) + UEFI and has the
full arm64 kexec feature set, so miniosv can be started from a running Linux
with `kexec` instead of by the firmware. The kernel config is derived from AWS
Graviton's (Amazon Linux 2023 `kernel6.18`, aarch64) and only enables hardware
miniosv itself supports: UEFI + ACPI, PSCI, GICv2/v3 + ITS, generic timer,
PMU, PCI (ECAM, MSI) and a PL011 or 16550 console. No storage or network
drivers (no NVMe, ENA, virtio); userspace is a built-in initramfs.

```
make arch=aarch64 app=scripts/linux-kexec/app          # -> build/release.aarch64/loader.kexec
scripts/linux-kexec/build.sh --payload build/release.aarch64/loader.kexec
scripts/linux-kexec/run.py                             # boot Linux, get a shell
scripts/linux-kexec/run.py --test file,load,kho,kdump  # Linux kexec self-test
scripts/linux-kexec/run.py --test miniosv-file         # kexec_file_load() into miniosv
scripts/linux-kexec/run.py --test miniosv-load         # kexec_load() (kexec-tools) into miniosv
```

## Files

| File | Purpose |
|---|---|
| `build.sh` | Downloads pinned sources (Linux 6.18.58, busybox, kexec-tools, the Graviton kernel RPM), derives the config, builds everything into `build/linux-kexec/`. |
| `graviton.symbols` | The Kconfig symbols copied verbatim from the Graviton config, grouped by subsystem. The build fails if a listed symbol is missing there or does not survive into the final `.config`. |
| `miniosv.config` | The deliberate departures from Graviton (KHO on, KASLR on, suspend instead of hibernation, no module compression), also checked. |
| `init` | PID 1 (busybox `sh`): mounts the pseudo filesystems, drops to a shell, or runs the `kxtest=` self-test chain. |
| `run.py` | QEMU launcher: AAVMF firmware (found like `scripts/run.py`), `-kernel Image` via the EFI stub, `-initrd payload.cpio.gz`. |

## How the config is derived

`allnoconfig` + the Graviton values of every symbol in `graviton.symbols` +
`miniosv.config`. Symbols named exactly must come out as copied; the errata
matched by `ARM64_ERRATUM_*` / `ARM64_WORKAROUND_*` are best-effort (a few
depend on 32-bit EL0 or KVM, which are off) and the build prints which were
dropped. The resulting `.config` is copied to `build/linux-kexec/linux.config`
and is also in the image as `/proc/config.gz`.

kexec features, all as on Graviton unless noted: `KEXEC` (kexec_load),
`KEXEC_FILE` (kexec_file_load), `KEXEC_SIG` + `KEXEC_IMAGE_VERIFY_SIG` (PE
signature check, not enforced: no `KEXEC_SIG_FORCE`, lockdown off),
`CRASH_DUMP` + `PROC_VMCORE` (kdump), and `KEXEC_HANDOVER` (not on Graviton;
boot with `kho=on`).

## Boot layout

The firmware loads `Image` as an EFI application (`-kernel` on QEMU virt
goes through the firmware's `fw_cfg` boot path), so Linux boots via its EFI
stub with the ACPI tables, exactly as miniosv does. The built-in initramfs
has busybox, `kexec`, and `init`; the separate `payload.cpio.gz` initrd
carries `/boot/Image` (a copy of the kernel, for the self-tests) and
`/boot/payload/<file>` for each `--payload` given to `build.sh`.

In the shell:

```
kexec -s -l /boot/Image --reuse-cmdline && kexec -e       # kexec_file_load
kexec -c -l /boot/Image --reuse-cmdline && kexec -e       # kexec_load (kexec-tools loader)
kexec -s -l /boot/payload/loader.efi --command-line=...   # a miniosv image, once it is kexec-able
```

## Self-test

`run.py --test file,load,kho,kdump` boots with `kxtest=file,load,kho,kdump`
(plus `kho=on kho_scratch=...` and `crashkernel=256M` when those steps are
present; see `run.py` for why the scratch sizes are explicit). Each
kernel in the chain checks the step that started it (`kxprev=`: the KHO
handover FDT is present, `/proc/vmcore` exists in the capture kernel) and
performs the next one. The last kernel prints `KEXEC-TEST: PASS` and powers
off; any failure prints `KEXEC-TEST: FAIL ...` and `run.py` exits 1.

## miniosv as the kexec target

`make arch=aarch64` also produces `build/<mode>/loader.kexec`, an arm64 Linux
"Image" (`boot/kexec/image.S`) wrapping the kernel ELF: a 64-byte Image
header, a `boot_info` slot, and the ELF itself at offset 0x10000, used in
place. Linux puts the image `text_offset = 0x80000` above a 2 MiB-aligned
base, so the ELF header lands at `+0x90000` and `text_start` at `+0xa0000`,
the geometry `arch/aarch64/boot.S` expects after a UEFI boot. The trampoline
fills `magic`, `kernel_phys_base` and `dtb_addr` and jumps to `uefi_entry`.

The kernel then (`arch/aarch64/kexec-boot.cc`, first thing in
`arch_init_early_console`) rebuilds the rest of `boot_info` from the device
tree Linux passed: the UEFI memory map and system table that Linux's EFI stub
preserved in `/chosen` give the memory map (classified like the UEFI stub
does, minus the DTB, `/memreserve/` entries, the initrd and Linux's EFI
memreserve chain - the GICv3 LPI tables live there) and, through the EFI
configuration tables, the ACPI RSDP and so the SPCR console UART; `bootargs`
is the command line. There is no wall clock after a kexec.

The GICv3 driver quiesces what Linux left running before programming it
(`GITS_CTLR` quiescent, `GICR_CTLR.EnableLPIs` cleared, pending SPIs/PPIs
cleared); clearing `EnableLPIs` is IMPLEMENTATION DEFINED and the kernel
aborts with a message if the hardware refuses, since adopting Linux's LPI
tables is not implemented. The physical timer is disarmed too.

Limits: the kexec'ing Linux must run at EL1 (QEMU `virt` without
`virtualization=on`; under `virtualization=on` Linux is an EL2 host and
kexec enters at EL2, which `boot.S` rejects), must have booted via UEFI
(the DTB needs the `linux,uefi-*` properties), and should not use `kho=on`
for the miniosv kexec (kexec_file then only places images in KHO scratch).

The `miniosv-file` / `miniosv-load` test steps kexec into
`/boot/payload/loader.kexec` with `kexec -s` / `kexec -c`; the miniosv built
with `app=scripts/linux-kexec/app` prints `KEXEC-TEST: PASS` and powers off.

## On a real Graviton (`aws/`)

`aws/main.tf` + `aws/test.sh` run the same chain on EC2: Terraform creates a
VPC/subnet, key pair, security group and one Amazon Linux 2023 arm64 instance
(its own Graviton kernel line, `al2023_kernel`, default 6.18), `test.sh run`
copies `Image` + `payload.cpio.gz` over, `kexec -s` loads the harness from the
AL2023 kernel, and the EC2 serial console (attached before `kexec -e`; the
buffered console output as a fallback) decides on the `KEXEC-TEST` lines.
`test.sh all` tears everything down afterwards, including the region's serial
console access if it had to enable it.

```
AWS_REGION=eu-central-1 INSTANCE_TYPE=c7g.4xlarge scripts/linux-kexec/aws/test.sh all --steps miniosv-file
```

Verified on a c7g.4xlarge (eu-central-1, AL2023 kernel 6.18.56): AL2023 ->
harness 6.18.58 -> miniosv (`16 cpus`, `KEXEC-TEST: PASS`, PSCI power-off).
The live serial console needs `ec2-instance-connect:SendSerialConsoleSSHPublicKey`
for the region's instances. c7gn is not offered in every region (not in
eu-central-1).
