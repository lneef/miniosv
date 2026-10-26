#!/usr/bin/env bash
#
# Build a minimal aarch64 Linux that boots under QEMU (TCG) + UEFI and can
# kexec into another kernel - the harness for kexec'ing miniosv from Linux.
#
# The kernel config is derived from AWS Graviton's: the symbols listed in
# graviton.symbols are copied from the Amazon Linux 2023 kernel6.18 aarch64
# config, the deliberate departures in miniosv.config go on top, and
# everything else starts from allnoconfig. Only hardware miniosv itself
# supports is enabled (UEFI/ACPI, PSCI, GIC + ITS, generic timer, PMU, PCI,
# PL011/16550); there are no storage or network drivers.
#
# Outputs, in build/linux-kexec/:
#   Image             the kernel (an EFI application) with a built-in
#                     initramfs: busybox, kexec-tools and the harness init
#   payload.cpio.gz   initrd with /boot/Image (a copy of the kernel, for
#                     kexec self-tests) and /boot/payload/* (--payload files)
#   linux.config      the derived kernel config
#
# Usage: build.sh [--payload FILE]...
#
# The kernel is built with LLVM (LLVM=1, like miniosv); busybox and
# kexec-tools are linked statically against glibc with a GNU cross compiler
# (CROSS_COMPILE, default aarch64-linux-gnu-). Sources are downloaded once
# into build/linux-kexec/dl and checked against pinned SHA-256 sums.
#
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
out="$root/build/linux-kexec"
jobs="${JOBS:-$(nproc)}"
cross="${CROSS_COMPILE:-aarch64-linux-gnu-}"

linux_ver=6.18.58
linux_sha=069b19f9032c4dac8149ae4b6520c35debca0f42a9f6d52ee55ba6ca9b818013
linux_url=https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$linux_ver.tar.xz

busybox_ver=1.38.0
busybox_sha=34f9ea6ff8636f2c9241153b9114eefa9e65674a45318ae1ef95bb5f31c53bb2
busybox_url=https://busybox.net/downloads/busybox-$busybox_ver.tar.bz2

kexec_ver=2.0.32
kexec_sha=8f81422a5fd2362cf6cb001b511e535565ed0f32c2f4451fb5eb68fed6710a5d
kexec_url=https://cdn.kernel.org/pub/linux/utils/kernel/kexec/kexec-tools-$kexec_ver.tar.xz

# The Graviton reference: Amazon Linux 2023's kernel6.18 aarch64 package. Its
# blobstore path is the package's SHA-256, so the URL pins the content too.
graviton_kver=6.18.54-121.166.amzn2023.aarch64
graviton_sha=7de525b5a290dadd6e6fbebecd64c61cf8185e6260a783596ddf00f09872364b
graviton_url=https://cdn.amazonlinux.com/al2023/blobstore/$graviton_sha/kernel6.18-$graviton_kver.rpm

payloads=()
while [ $# -gt 0 ]; do
    case "$1" in
    --payload)
        [ -f "${2:-}" ] || { echo "build.sh: --payload needs an existing file" >&2; exit 1; }
        payloads+=("$(cd "$(dirname "$2")" && pwd)/$(basename "$2")")
        shift 2
        ;;
    -h|--help)
        sed -n '2,/^set -euo/{/^set -euo/d;s/^# \{0,1\}//;p}' "$0"
        exit 0
        ;;
    *)
        echo "build.sh: unknown argument '$1' (see --help)" >&2
        exit 1
        ;;
    esac
done

missing=()
for tool in curl bsdtar sha256sum make flex bison bc perl openssl gzip \
            clang ld.lld llvm-objcopy "${cross}gcc"; do
    command -v "$tool" >/dev/null 2>&1 || missing+=("$tool")
done
if [ ${#missing[@]} -gt 0 ]; then
    echo "build.sh: missing host tools: ${missing[*]}" >&2
    exit 1
fi

step() { echo "==> $*"; }

# fetch <url> <sha256>: download into $out/dl once, verify every time.
fetch() {
    local url=$1 sha=$2 file="$out/dl/${1##*/}"
    mkdir -p "$out/dl"
    if [ ! -f "$file" ]; then
        step "fetch ${url##*/}"
        curl -fL --retry 3 -o "$file.part" "$url"
        mv "$file.part" "$file"
    fi
    echo "$sha  $file" | sha256sum -c --quiet - ||
        { echo "build.sh: checksum mismatch for $file" >&2; exit 1; }
}

# unpack <tarball> <dir>: extract once into $out/src.
unpack() {
    local tarball=$1 dir=$2
    if [ ! -d "$out/src/$dir" ]; then
        step "unpack $dir"
        mkdir -p "$out/src"
        tar -xf "$out/dl/$tarball" -C "$out/src"
    fi
}

fetch "$linux_url" "$linux_sha"
fetch "$busybox_url" "$busybox_sha"
fetch "$kexec_url" "$kexec_sha"
fetch "$graviton_url" "$graviton_sha"

unpack "linux-$linux_ver.tar.xz" "linux-$linux_ver"
unpack "busybox-$busybox_ver.tar.bz2" "busybox-$busybox_ver"
unpack "kexec-tools-$kexec_ver.tar.xz" "kexec-tools-$kexec_ver"

graviton_config="$out/graviton-$graviton_kver.config"
if [ ! -f "$graviton_config" ]; then
    bsdtar -xOf "$out/dl/kernel6.18-$graviton_kver.rpm" \
        "./lib/modules/$graviton_kver/config" > "$graviton_config"
fi

# --- busybox (static) ---------------------------------------------------------
bb_src="$out/src/busybox-$busybox_ver"
bb_obj="$out/obj/busybox"
if [ ! -x "$bb_obj/busybox" ]; then
    step "busybox $busybox_ver"
    mkdir -p "$bb_obj"
    make -C "$bb_src" O="$bb_obj" -s defconfig
    # tc no longer builds against current kernel headers (CBQ was removed).
    sed -i -e 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' \
           -e 's/^CONFIG_TC=y/# CONFIG_TC is not set/' "$bb_obj/.config"
    make -C "$bb_obj" -s oldconfig < /dev/null > /dev/null
    make -C "$bb_obj" -s -j"$jobs" CROSS_COMPILE="$cross" busybox
fi

# --- kexec-tools (static) -------------------------------------------------------
kx_obj="$out/obj/kexec-tools"
if [ ! -x "$kx_obj/build/sbin/kexec" ]; then
    step "kexec-tools $kexec_ver"
    rm -rf "$kx_obj"
    mkdir -p "$out/obj"
    cp -a "$out/src/kexec-tools-$kexec_ver" "$kx_obj"
    (cd "$kx_obj" &&
        ./configure -q --host="${cross%-}" CC="${cross}gcc" LDFLAGS=-static \
            --without-zlib --without-lzma --without-xen &&
        make -s -j"$jobs" &&
        "${cross}strip" build/sbin/kexec)
fi

# --- built-in initramfs ---------------------------------------------------------
# A gen_init_cpio list rather than a directory, so /dev/console exists before
# devtmpfs is mounted without needing root to mknod it.
step "initramfs"
cat > "$out/initramfs.list" <<EOF
dir /bin 0755 0 0
dir /sbin 0755 0 0
dir /dev 0755 0 0
nod /dev/console 0600 0 0 c 5 1
dir /proc 0555 0 0
dir /sys 0555 0 0
dir /tmp 1777 0 0
dir /boot 0755 0 0
file /init $here/init 0755 0 0
file /bin/busybox $bb_obj/busybox 0755 0 0
slink /bin/sh busybox 0777 0 0
file /sbin/kexec $kx_obj/build/sbin/kexec 0755 0 0
EOF

# --- kernel config --------------------------------------------------------------
linux_src="$out/src/linux-$linux_ver"
linux_obj="$out/obj/linux"
kmake=(make -C "$linux_src" O="$linux_obj" ARCH=arm64 LLVM=1 -j"$jobs")
mkdir -p "$linux_obj"

step "derive kernel config from $(basename "$graviton_config")"
# Copy each listed symbol's Graviton value; 'm' becomes 'y'. Symbols named
# exactly go to graviton.fragment and must come out as copied; those matched
# by a prefix* line go to graviton-glob.fragment and only count where their
# dependencies are met (e.g. the 32-bit EL0 and KVM errata are dropped).
awk -v ref="$graviton_config" -v globout="$out/graviton-glob.fragment" '
    BEGIN {
        while ((getline line < ref) > 0) {
            if (match(line, /^CONFIG_[A-Za-z0-9_]+=/)) {
                sym = substr(line, 8, RLENGTH - 8)
                val[sym] = line
                order[++n] = sym
            } else if (match(line, /^# CONFIG_[A-Za-z0-9_]+ is not set$/)) {
                sym = substr(line, 10, length(line) - 9 - 11)
                val[sym] = line
                order[++n] = sym
            }
        }
    }
    function emit(sym, file) {
        line = val[sym]
        sub(/=m$/, "=y", line)
        if (file == "") print line
        else print line > file
    }
    { sub(/#.*/, ""); gsub(/[ \t]/, "") }
    $0 == "" { next }
    /\*$/ {
        prefix = substr($0, 1, length($0) - 1)
        hit = 0
        for (i = 1; i <= n; i++)
            if (index(order[i], prefix) == 1) { emit(order[i], globout); hit = 1 }
        if (!hit) { print "graviton.symbols: no symbol matches " $0 > "/dev/stderr"; bad = 1 }
        next
    }
    {
        if (!($0 in val)) { print "graviton.symbols: " $0 " is not in the Graviton config" > "/dev/stderr"; bad = 1; next }
        emit($0, "")
    }
    END { exit bad }
' "$here/graviton.symbols" > "$out/graviton.fragment"

{
    cat "$out/graviton.fragment"
    grep -E '^(CONFIG_|# CONFIG_)' "$here/miniosv.config"
    echo "CONFIG_INITRAMFS_SOURCE=\"$out/initramfs.list\""
} > "$out/requested.config"

cat "$out/graviton-glob.fragment" "$out/requested.config" > "$out/allconfig"
"${kmake[@]}" -s KCONFIG_ALLCONFIG="$out/allconfig" allnoconfig

# Every requested value must have survived the dependency resolution. The last
# assignment of a symbol wins, as it does for KCONFIG_ALLCONFIG.
awk '
    function sym_of(line) {
        if (match(line, /^CONFIG_[A-Za-z0-9_]+=/)) return substr(line, 8, RLENGTH - 8)
        if (match(line, /^# CONFIG_[A-Za-z0-9_]+ is not set$/)) return substr(line, 10, length(line) - 20)
        return ""
    }
    FNR == NR { s = sym_of($0); if (s != "") { want[s] = $0; if (!(s in seen)) { seen[s] = 1; order[++n] = s } } next }
    { s = sym_of($0); if (s != "") have[s] = $0 }
    END {
        for (i = 1; i <= n; i++) {
            s = order[i]; w = want[s]
            h = (s in have) ? have[s] : "# CONFIG_" s " is not set"
            if (w != h) { printf "  wanted: %s\n  got:    %s\n", w, h > "/dev/stderr"; bad = 1 }
        }
        exit bad
    }
' "$out/requested.config" "$linux_obj/.config" ||
    { echo "build.sh: the kernel config above did not come out as requested" >&2; exit 1; }
dropped=$(grep '=y$' "$out/graviton-glob.fragment" | cut -d= -f1 | while read -r sym; do
    grep -q "^$sym=y$" "$linux_obj/.config" || printf ' %s' "${sym#CONFIG_}"
done)
[ -z "$dropped" ] || echo "    Graviton symbols whose dependencies are off here:$dropped"
cp "$linux_obj/.config" "$out/linux.config"

# --- kernel ---------------------------------------------------------------------
step "linux $linux_ver"
"${kmake[@]}" -s Image
cp "$linux_obj/arch/arm64/boot/Image" "$out/Image"

# --- payload initrd ---------------------------------------------------------------
step "payload.cpio.gz"
{
    echo "dir /boot 0755 0 0"
    echo "file /boot/Image $out/Image 0644 0 0"
    if [ ${#payloads[@]} -gt 0 ]; then
        echo "dir /boot/payload 0755 0 0"
        for p in "${payloads[@]}"; do
            echo "file /boot/payload/$(basename "$p") $p 0644 0 0"
        done
    fi
} > "$out/payload.list"
"$linux_obj/usr/gen_init_cpio" "$out/payload.list" | gzip -9 > "$out/payload.cpio.gz"

step "done"
ls -l "$out/Image" "$out/payload.cpio.gz" "$out/linux.config"
