/*
 * Copyright (C) 2026 miniosv contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

/* The kexec Image (boot/kexec/image.S) is a flat binary: everything in one
   section at address 0, turned into loader.kexec with llvm-objcopy -O binary.
   Only PC-relative code runs from it, so the link address does not matter. */

ENTRY(_head)

SECTIONS
{
    . = 0;
    .head : { *(.head) }
    /DISCARD/ : { *(.note*) *(.comment) *(.eh_frame) }
}
