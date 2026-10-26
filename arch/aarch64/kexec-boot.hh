/*
 * Copyright (C) 2026 miniosv contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef KEXEC_BOOT_HH
#define KEXEC_BOOT_HH

// When the kernel was started by Linux kexec (boot/kexec/image.S), complete
// osv_boot_info from the device tree Linux handed over. A no-op after a UEFI
// boot. Must run before anything reads boot_info, i.e. first thing in
// arch_init_early_console(); it cannot print, so a failure is recorded and
// reported by kexec_boot_error() once the console is up.
void kexec_boot_prepare();
const char *kexec_boot_error();

#endif /* KEXEC_BOOT_HH */
