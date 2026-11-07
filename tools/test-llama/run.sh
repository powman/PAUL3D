#!/usr/bin/env bash
# PC check of the text engine: llama.cpp (scripts/llama-fetch.sh) on the unified ggml (scripts/ggml-unified.sh), CPU,
# with a small model of the same family as the console's (Qwen3.5 0.8B). Work and cache in ${TMPDIR:-/tmp}/paul3d-llama.
#   bash tools/test-llama/run.sh
set -u
root=$(cd "$(dirname "$0")/../.." && pwd); w=${TMPDIR:-/tmp}/paul3d-llama; mkdir -p "$w"
fail=0
t() { if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: expected [$2], got [$3]"; fail=1; fi; }
model=$w/Qwen3.5-0.8B-Q4_K_M.gguf
[ -s "$model" ] || curl -sSL --fail -o "$model" \
  https://huggingface.co/unsloth/Qwen3.5-0.8B-GGUF/resolve/6ab461498e2023f6e3c1baea90a8f0fe38ab64d0/Qwen3.5-0.8B-Q4_K_M.gguf
bash "$root/scripts/ggml-unified.sh" "$w/ggml" "$root/third_party/PS5SD/patches/ggml" > "$w/ggml.log" 2>&1; t "unified ggml" 0 "$?"
bash "$root/scripts/llama-fetch.sh" "$w/llama" > "$w/fetch1.log" 2>&1; t "llama-fetch" 0 "$?"
t "llama pinned" 1 "$(grep -c '^llama 4ed1c8507e78c5dddc8e48d2822b87c2099e00d1 ' "$w/llama/.paul3d-llama" 2>/dev/null)"
t "algorithm include added" 1 "$(head -1 "$w/llama/src/models/clef.cpp" 2>/dev/null | grep -c '#include <algorithm>')"
t "io patch in place" 1 "$(grep -c '_IOFBF, 16 << 20' "$w/llama/src/llama-mmap.cpp" 2>/dev/null)"
before=$(git -C "$w/llama" status --porcelain 2>/dev/null | md5sum)
bash "$root/scripts/llama-fetch.sh" "$w/llama" > "$w/fetch2.log" 2>&1; t "llama-fetch again" 0 "$?"
t "second fetch changes nothing" "$before" "$(git -C "$w/llama" status --porcelain 2>/dev/null | md5sum)"
cmake -S "$root/tools/test-llama" -B "$w/build" -DCMAKE_BUILD_TYPE=Release -DGGML_DIR="$w/ggml" -DLLAMA_DIR="$w/llama" \
  -DPAUL3D_PS5="$root/ps5" > "$w/cmake.log" 2>&1 && cmake --build "$w/build" -j"$(nproc)" > "$w/build.log" 2>&1
t "build" 0 "$?"; [ "$fail" = 0 ] || { grep -m 15 "error" "$w/build.log"; }
out=$("$w/build/hello" "$model" "Reply with exactly the word OK." 2> "$w/hello.err")
t "hello runs" 0 "$?"
t "architecture qwen35" 1 "$(grep -c '^arch=qwen35$' <<<"$out")"
t "answer contains OK" 1 "$(grep -c '^answer=.*OK' <<<"$out")"
if [ -x "$w/build/test_text" ]; then "$w/build/test_text" "$model"; t "test_text" 0 "$?"; fi
exit $fail
