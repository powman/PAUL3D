#!/usr/bin/env bash
# Checks scripts/ggml-unified.sh: builds the unified ggml in a temporary folder, runs it again (nothing changes),
# and with a patch that cannot apply (stops with a clear message, exit 1).
#   bash tools/test-ggml/unified.sh        (needs network once: clones leejet/ggml and fetches pwilkin/ggml)
set -u
here=$(cd "$(dirname "$0")" && pwd); root=$(cd "$here/../.." && pwd)
script="$root/scripts/ggml-unified.sh"; patches="$root/third_party/PS5SD/patches/ggml"
d=$(mktemp -d); trap 'rm -rf "$d"' EXIT
fail=0
t() { if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: expected [$2], got [$3]"; fail=1; fi; }

bash "$script" "$d/g" "$patches" > "$d/log1" 2>&1; t "first run succeeds" 0 "$?"
t "summary counts what was left out" 1 "$(grep -c '0 pwilkin applied, 10 left out (9 cuda-only, 1 windows-arm64)' "$d/log1")"
t "10 pwilkin commits accounted for" 10 "$(grep -Ec '^pwilkin(-skipped)? ' "$d/g/.paul3d-ggml" 2>/dev/null)"
t "only CUDA-only commits skipped" 0 "$(sed -n 's/^pwilkin-skipped \([0-9a-f]*\) cuda-only$/\1/p' "$d/g/.paul3d-ggml" | while read c; do git -C "$d/g" show --name-only --format= "$c" | grep -vc '^src/ggml-cuda/'; done | awk '{s+=$1} END {print s+0}')"
t "the Windows-ARM64 commit is skipped with its reason" 1 "$(grep -c '^pwilkin-skipped 9dc0e51f8ee30c7295d2d9b59e788024db983bd9 windows-arm64$' "$d/g/.paul3d-ggml" 2>/dev/null)"
t "every PS5SD patch recorded" "$(ls "$patches"/*.patch | wc -l)" "$(grep -c '^ps5sd ' "$d/g/.paul3d-ggml" 2>/dev/null)"
t "leejet base recorded" 1 "$(grep -c '^leejet dc998771e700d49bbdf9a39cede22bb5b3311767$' "$d/g/.paul3d-ggml" 2>/dev/null)"
t "ggml 0.27" 1 "$(grep -c 'set(GGML_VERSION_MINOR 27)' "$d/g/CMakeLists.txt" 2>/dev/null)"
before=$(git -C "$d/g" status --porcelain | sort | md5sum)
bash "$script" "$d/g" "$patches" > "$d/log2" 2>&1; t "second run succeeds" 0 "$?"
t "second run changes nothing" "$before" "$(git -C "$d/g" status --porcelain | sort | md5sum)"

# A patch that cannot apply: the first PS5SD patch twice, under another name.
mkdir -p "$d/bad"; cp "$patches"/*.patch "$d/bad/"; first=$(ls "$patches"/*.patch | head -1)
cp "$first" "$d/bad/9999-again.patch"
bash "$script" "$d/g2" "$d/bad" > "$d/log3" 2>&1; t "a patch that does not apply fails" 1 "$?"
t "the message names it" 1 "$(grep -c "ggml-unified: 9999-again.patch não aplica" "$d/log3")"
t "no marker file after a failure" 1 "$([ ! -f "$d/g2/.paul3d-ggml" ] && echo 1 || echo 0)"
exit $fail
