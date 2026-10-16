#!/usr/bin/env bash
# Builds the pinned DirectX Shader Compiler from its open-source tree (University of
# Illinois/NCSA licence, see LICENSE.TXT and ThirdPartyNotices.txt in the source) so that no
# Microsoft binary-package terms apply on non-Windows hosts (docs/decisions/ADR-015).
#
# Usage: scripts/build_dxc_from_source.sh [install-root]   (default: build/tools)
# Result: <install-root>/dxc-<rev12>/bin/dxc, picked up automatically by cmake/Dependencies.cmake.
# Cost: roughly 0.5-1.5 h and ~6 GB on a 4-core machine; needed once per host.
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$(pwd)"
LOCK="$ROOT/dependencies.lock.json"
read -r ORIGIN TAG REV < <(python3 -c "
import json; d=[x for x in json.load(open('$LOCK'))['dependencies'] if x['name']=='dxc'][0]['source_build']
print(d['origin'], d['selected_tag'], d['revision'])")
DEST="${1:-$ROOT/build/tools}/dxc-${REV:0:12}"
SRC="$DEST/src"; BLD="$DEST/build"
mkdir -p "$DEST"
if [ ! -d "$SRC/.git" ]; then
  git clone --depth 1 --branch "$TAG" --recurse-submodules --shallow-submodules "$ORIGIN.git" "$SRC"
fi
HEAD=$(git -C "$SRC" rev-parse HEAD)
[ "$HEAD" = "$REV" ] || { echo "DXC tag $TAG resolved to $HEAD, lock pins $REV; refusing to build" >&2; exit 1; }
cmake -S "$SRC" -B "$BLD" -G Ninja -C "$SRC/cmake/caches/PredefinedParams.cmake" \
  -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=OFF \
  -DHLSL_INCLUDE_TESTS=OFF -DSPIRV_BUILD_TESTS=OFF -DLLVM_INCLUDE_TESTS=OFF -DCLANG_INCLUDE_TESTS=OFF \
  -DLLVM_INCLUDE_EXAMPLES=OFF -DLLVM_INCLUDE_DOCS=OFF -DCMAKE_INSTALL_PREFIX="$DEST"
ninja -C "$BLD" dxc
mkdir -p "$DEST/bin" "$DEST/lib" "$DEST/licenses"
cp "$BLD/bin/dxc" "$DEST/bin/"
cp "$BLD"/lib/libdxcompiler.so* "$DEST/lib/" 2>/dev/null || true
cp "$SRC/LICENSE.TXT" "$SRC/ThirdPartyNotices.txt" "$DEST/licenses/"
"$DEST/bin/dxc" --version
echo "$REV" > "$DEST/REVISION"
echo "DXC built from source: $DEST/bin/dxc"
