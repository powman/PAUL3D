#!/usr/bin/env bash
# The one ggml that the three engines of PAUL3D build against (docs/superpowers/specs/2026-11-07-paul3d-all-in-one-design.md):
# leejet/ggml as stable-diffusion.cpp pins it (upstream ggml 0.27.0 + leejet's ops), the commits pwilkin/ggml adds for
# trellis.cpp, and PS5SD's ggml patches for the console. pwilkin's commits that only touch src/ggml-cuda/ are left out
# (recorded as pwilkin-skipped): one of them does not apply on 0.27, and CUDA is built neither for the console (Vulkan)
# nor for the CPU check on a PC.
#   scripts/ggml-unified.sh <dest> <patches-dir>
# <dest>/.paul3d-ggml lists what went in; with the same list there already, nothing is done (the CI reuses .deps).
# A commit or a patch that does not apply stops it with a message and exit 1, and <dest> gets no .paul3d-ggml.
set -euo pipefail
dest=${1:?usage: ggml-unified.sh <dest> <patches-dir>}
patches=${2:?usage: ggml-unified.sh <dest> <patches-dir>}
LEEJET=dc998771e700d49bbdf9a39cede22bb5b3311767      # leejet/stable-diffusion.cpp @ b13a482's ggml
PW_BASE=456172ec733a135778adcd32d00e576a58232e45     # upstream "ggml : bump version to 0.24.0", pwilkin's base
PW_HEAD=f8697b33795db2ee6a2569b40853ca6aaa69f9b9     # pwilkin/ggml as trellis.cpp @ 8e4ce11 pins it
# Left out by hand, with the reason: "Fix Qualcomm HTP and Vulkan execution on Windows ARM64" conflicts with 0.27's Vulkan
# backend and fixes neither the console nor a PC on x86.
PW_SKIP="9dc0e51f8ee30c7295d2d9b59e788024db983bd9:windows-arm64"

patch_names() { for p in "$patches"/*.patch; do [ -e "$p" ] && basename "$p"; done; }

marker=$dest/.paul3d-ggml
if [ -f "$marker" ] && [ "$(sed -n 1p "$marker")" = "leejet $LEEJET" ] &&
   [ "$(grep -Ec '^pwilkin(-skipped)? ' "$marker")" = 10 ] && grep -Eq "^pwilkin(-skipped)? $PW_HEAD" "$marker" &&
   [ "$(sed -n 's/^ps5sd //p' "$marker")" = "$(patch_names)" ]; then
    echo "ggml-unified: $dest is already built"
    exit 0
fi

rm -rf "$dest"
git init -q "$dest"
g() { git -C "$dest" -c user.name=paul3d -c user.email=paul3d@localhost -c advice.detachedHead=false "$@"; }
g fetch -q --depth 1 https://github.com/leejet/ggml "$LEEJET"
g checkout -q FETCH_HEAD
g fetch -q --depth 12 https://github.com/pwilkin/ggml "$PW_HEAD"
list=$(g rev-list --reverse "$PW_BASE..$PW_HEAD")
{ echo "leejet $LEEJET"; } > "$dest/.paul3d-ggml.part"
for c in $list; do
    reason=$(printf '%s\n' $PW_SKIP | sed -n "s/^$c://p")
    if [ -n "$reason" ]; then
        echo "pwilkin-skipped $c $reason" >> "$dest/.paul3d-ggml.part"
        continue
    fi
    if [ -z "$(g show --name-only --format= "$c" | grep -v '^src/ggml-cuda/')" ]; then
        echo "pwilkin-skipped $c cuda-only" >> "$dest/.paul3d-ggml.part"
        continue
    fi
    if ! g cherry-pick "$c" > /dev/null 2>&1; then
        g cherry-pick --abort > /dev/null 2>&1 || true
        echo "ggml-unified: pwilkin $c não aplica sobre leejet ${LEEJET:0:8}" >&2
        rm -f "$dest/.paul3d-ggml.part"
        exit 1
    fi
    echo "pwilkin $c" >> "$dest/.paul3d-ggml.part"
done
for name in $(patch_names); do
    if ! g apply "$patches/$name" 2> /dev/null; then
        echo "ggml-unified: $name não aplica sobre leejet ${LEEJET:0:8} + pwilkin" >&2
        rm -f "$dest/.paul3d-ggml.part"
        exit 1
    fi
    echo "ps5sd $name" >> "$dest/.paul3d-ggml.part"
done
mv "$dest/.paul3d-ggml.part" "$marker"
why=$(sed -n 's/^pwilkin-skipped [0-9a-f]* //p' "$marker" | sort | uniq -c | awk '{printf "%s%d %s", (NR > 1 ? ", " : ""), $1, $2}')
echo "ggml-unified: $dest = leejet ${LEEJET:0:8}, $(grep -c '^pwilkin ' "$marker") pwilkin applied, $(grep -c '^pwilkin-skipped ' "$marker") left out ($why), $(patch_names | wc -l) PS5SD patch(es)"
