#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# dav1d for the PS5: FFmpeg's libdav1d wrapper is how Switch 2 AV1 movies decode.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
scratch=$(cat "$root/.local/headless-cache")
export PS5_PAYLOAD_SDK="$scratch/sdk"
version=1.5.1
source="$root/.deps/dav1d-$version"
if [[ ! -f $source/meson.build ]]; then
    mkdir -p "$root/.deps"
    curl -sSfL "https://github.com/videolan/dav1d/archive/refs/tags/$version.tar.gz" -o "$root/.deps/dav1d-$version.tar.gz"
    tar -xf "$root/.deps/dav1d-$version.tar.gz" -C "$root/.deps"
fi
build="$scratch/dav1d-native"
rm -rf "$build"
mkdir -p "$build"
cat > "$build/cross.txt" <<CROSS
[binaries]
c = ['bash', '$root/tools/ffmpeg-ps5-cc.sh']
ar = 'llvm-ar-18'
strip = 'llvm-strip-18'
nasm = 'nasm'
pkg-config = 'pkg-config'
[host_machine]
system = 'freebsd'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'
CROSS
meson setup "$build/b" "$source" --cross-file "$build/cross.txt" --prefix "$build/install" --libdir lib \
    --buildtype release --default-library static -Db_staticpic=true -Denable_tools=false -Denable_tests=false \
    -Dbitdepths=8,16 -Dlogging=false
ninja -C "$build/b" -j"${EDEN_BUILD_JOBS:-$(nproc)}"
ninja -C "$build/b" install
