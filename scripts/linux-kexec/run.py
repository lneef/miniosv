#!/usr/bin/env python3

# Boot the Linux kexec harness (build.sh) under QEMU + UEFI on aarch64.
#
# The firmware (AAVMF, found the same way as scripts/run.py) loads the kernel
# from QEMU's -kernel as an EFI application, so Linux boots through its EFI
# stub with the ACPI tables - the same UEFI/ACPI hand-off miniosv gets. There
# is no disk: userspace is the kernel's built-in initramfs, and the -initrd
# payload only carries the files to kexec.
#
# Interactive by default (Ctrl-A X quits). With --test, runs the kexec
# self-test chain unattended and exits 0 on "KEXEC-TEST: PASS".

import argparse
import importlib.util
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time

here = os.path.dirname(os.path.abspath(__file__))

# scripts/run.py, for its firmware lookup and pflash setup. Loaded by path:
# a plain "import run" would find this file first.
_spec = importlib.util.spec_from_file_location(
    "miniosv_run", os.path.join(here, "..", "run.py"))
run = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(run)

out = os.path.join(here, '..', '..', 'build', 'linux-kexec')


def qemu_command(opts, code, vars_):
    cmdline = ["console=ttyAMA0", "earlycon"]
    steps = opts.test.split(',') if opts.test else []
    if 'kho' in steps or opts.kho:
        # Explicit scratch sizes: the automatic sizing ("200% of the memory
        # reserved by the kernel", split low/global) comes out as a zero
        # global area when all RAM is below 4 GiB, and KHO then disables
        # itself. The KHO-booted kernel takes its early allocations from the
        # scratch areas only, so the global one must also hold crashkernel=.
        cmdline += ["kho=on", "kho_scratch=%s" % opts.kho_scratch]
    if 'kdump' in steps or opts.crashkernel:
        cmdline.append("crashkernel=%s" % (opts.crashkernel or "256M"))
    if steps:
        cmdline.append("kxtest=%s" % opts.test)
    if opts.append:
        cmdline.append(opts.append)

    return [
        opts.qemu_path or "qemu-system-aarch64",
        "-machine", "virt,gic-version=%s" % opts.gic_version,
        # pauth-impdef: QARMA pointer authentication is very slow under TCG.
        "-cpu", "max,pauth-impdef=on",
        "-m", opts.memsize, "-smp", opts.vcpus,
        "-drive", "if=pflash,format=raw,readonly=on,file=%s" % code,
        "-drive", "if=pflash,format=raw,file=%s" % vars_,
        "-kernel", opts.kernel, "-initrd", opts.initrd,
        "-append", " ".join(cmdline),
        "-nic", "none", "-display", "none", "-no-reboot",
    ] + (["-serial", "stdio", "-monitor", "none"] if opts.test
         else ["-serial", "mon:stdio"]) + [a for arg in opts.pass_args or [] for a in arg.split()]


def run_test(cmd, timeout):
    "Run QEMU headless, echo the console, and decide on the KEXEC-TEST lines."
    proc = subprocess.Popen(cmd, stdin=subprocess.DEVNULL,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    result = {}

    def reader():
        for raw in proc.stdout:
            line = raw.decode(errors='replace')
            sys.stdout.write(line)
            sys.stdout.flush()
            if "KEXEC-TEST: PASS" in line:
                result['pass'] = True
            elif "KEXEC-TEST: FAIL" in line:
                result['fail'] = line.strip()

    t = threading.Thread(target=reader, daemon=True)
    t.start()
    deadline = time.monotonic() + timeout
    while proc.poll() is None and time.monotonic() < deadline:
        if 'fail' in result:
            break
        time.sleep(0.2)
    if proc.poll() is None:
        proc.kill()
    proc.wait()
    t.join(timeout=5)

    if result.get('pass'):
        print("\nrun.py: kexec test passed")
        return 0
    if 'fail' in result:
        print("\nrun.py: kexec test failed: %s" % result['fail'])
    elif time.monotonic() >= deadline:
        print("\nrun.py: kexec test timed out after %ds" % timeout)
    else:
        print("\nrun.py: QEMU exited without a test result")
    return 1


def main():
    p = argparse.ArgumentParser(
        description="Boot the Linux kexec harness under QEMU + UEFI (aarch64).")
    p.add_argument("--test", metavar="STEPS",
                   help="run the kexec self-test chain unattended, e.g. "
                        "file,load,kho,kdump (adds kho=on / crashkernel= as needed)")
    p.add_argument("--timeout", type=int, default=600,
                   help="seconds before --test gives up (default 600)")
    p.add_argument("--kho", action="store_true", help="boot with kho=on")
    p.add_argument("--kho-scratch", default="16M,512M,128M", metavar="LOW,GLOBAL,NODE",
                   help="KHO scratch area sizes (default 16M,512M,128M)")
    p.add_argument("--crashkernel", metavar="SIZE",
                   help="reserve crash kernel memory, e.g. 256M")
    p.add_argument("--append", help="extra kernel command line")
    p.add_argument("-m", "--memsize", default="2G", help="guest memory (default 2G)")
    p.add_argument("-c", "--vcpus", default="2", help="number of vcpus (default 2)")
    p.add_argument("--gic-version", default="3", help="GIC version (default 3)")
    p.add_argument("--kernel", default=os.path.join(out, "Image"))
    p.add_argument("--initrd", default=os.path.join(out, "payload.cpio.gz"))
    p.add_argument("--qemu-path", help="path to qemu-system-aarch64")
    p.add_argument("--pass-args", action="append",
                   help="append raw arguments to the QEMU command line")
    p.add_argument("--dry-run", action="store_true",
                   help="print the QEMU command line and exit")
    opts = p.parse_args()

    for f in (opts.kernel, opts.initrd):
        if not os.path.exists(f):
            sys.exit("%s does not exist. Build it first: scripts/linux-kexec/build.sh" % f)

    code, vars_ = run.find_firmware('aarch64')
    workdir = tempfile.mkdtemp(prefix='linux-kexec-run-')
    try:
        # Fresh variable store per run: nothing in it is worth keeping, and a
        # stale boot order would only slow the firmware down.
        code_copy, vars_copy = run.setup_pflash('aarch64', code, vars_, workdir, workdir)
        cmd = qemu_command(opts, code_copy, vars_copy)
        if opts.dry_run:
            print(run.format_args(cmd))
            return 0
        if opts.test:
            return run_test(cmd, opts.timeout)
        run.stty_save()
        try:
            return subprocess.call(cmd)
        finally:
            run.stty_restore()
    finally:
        shutil.rmtree(workdir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
