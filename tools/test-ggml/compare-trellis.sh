#!/usr/bin/env bash
# The CPU check of a new ggml (phase 0 of docs/superpowers/specs/2026-11-07-paul3d-all-in-one-design.md): builds trellis.cpp
# twice on this PC, with today's ggml (pwilkin/ggml as trellis.cpp pins it + PS5SD's ggml patches) and with the unified one
# (scripts/ggml-unified.sh), makes the same mesh with both and compares them (tools/compare-glb.py).
#   tools/test-ggml/compare-trellis.sh <models-dir>      e.g. ~/Downloads/PS5MG-main/models/q4
# Builds go to ${TMPDIR:-/tmp}/paul3d-ggml-check. CPU only (--gpu -1), 512, seed 42, 4 steps, no texture, 20000 faces.
# Only the portable patches of the title are applied (patch-notex-faces.py, patch-multiview.py): the others need the
# console's headers (paul3d_threads.h, mg_pread_full).
set -euo pipefail
models=${1:?usage: compare-trellis.sh <models-dir>}
root=$(cd "$(dirname "$0")/../.." && pwd)
work=${TMPDIR:-/tmp}/paul3d-ggml-check
TRELLIS=8e4ce11a77d35c3e3a4b3f84a29d92bf5491e64e
PW_HEAD=f8697b33795db2ee6a2569b40853ca6aaa69f9b9
patches=$root/third_party/PS5SD/patches/ggml
mkdir -p "$work"

build() {  # build <name>: $work/<name>/build/trellis-cli
    local name=$1 t=$work/$1
    if [ ! -d "$t/.git" ]; then
        git clone -q https://github.com/pwilkin/trellis.cpp "$t"
        git -C "$t" checkout -q "$TRELLIS"
        git -C "$t" submodule update -q --init --depth 1 thirdparty/ggml 2>/dev/null || true
    fi
    python3 "$root/ps5/patch-notex-faces.py" "$t/src/trellis_cli.cpp" > /dev/null
    python3 "$root/ps5/patch-multiview.py" "$t" > /dev/null
    local g=$t/thirdparty/ggml
    if [ "$name" = current ]; then
        if [ ! -f "$g/.paul3d-current" ]; then
            rm -rf "$g"; git clone -q https://github.com/pwilkin/ggml "$g"; git -C "$g" checkout -q "$PW_HEAD"
            for p in "$patches"/*.patch; do git -C "$g" apply "$p"; done
            touch "$g/.paul3d-current"
        fi
    else
        bash "$root/scripts/ggml-unified.sh" "$g" "$patches"
    fi
    cmake -S "$t" -B "$t/build" -DCMAKE_BUILD_TYPE=Release > "$t/cmake.log" 2>&1
    if ! cmake --build "$t/build" --target trellis-cli -j"$(nproc)" > "$t/build.log" 2>&1; then
        echo "BUILD FAILED ($name): $t/build.log"; grep -m 20 -E "error" "$t/build.log"; return 1
    fi
}

run() {  # run <name>: $work/<name>.glb, $work/<name>.log
    local t=$work/$1
    /usr/bin/time -f "%e s, %M KB" "$t/build/trellis-cli" "$t/assets/goblin.png" "$work/$1.glb" -m "$models" --gpu -1 \
        --res 512 -s 42 --steps 4 --no-texture --faces 20000 > "$work/$1.log" 2>&1
    echo "$1: $(tail -1 "$work/$1.log")"
    grep -E "^\[[0-9]/[0-9]\]" "$work/$1.log" | sed "s/^/  $1 /" || true
}

build current
build unified
run current
run unified
python3 "$root/tools/compare-glb.py" "$work/current.glb" "$work/unified.glb"
