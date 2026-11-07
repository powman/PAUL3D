#!/usr/bin/env bash
# Builds and runs tools/test-queue/test.cpp against ps5/mg_queue.cpp on the PC.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd); b=$(mktemp -d); trap 'rm -rf "$b"' EXIT
g++ -std=c++17 -Wall -I "$root/ps5" "$root/tools/test-queue/test.cpp" "$root/ps5/mg_queue.cpp" -o "$b/t" || exit 1
"$b/t" || fail=1
# mg_textjob needs nlohmann/json from llama.cpp's vendor/ (scripts/llama-fetch.sh, cached in ${TMPDIR:-/tmp}/paul3d-llama).
ll=${TMPDIR:-/tmp}/paul3d-llama/llama
bash "$root/scripts/llama-fetch.sh" "$ll" > /dev/null || exit 1
g++ -std=c++17 -Wall -I "$root/ps5" -I "$ll/vendor" "$root/tools/test-queue/test_textjob.cpp" "$root/ps5/mg_textjob.cpp" -o "$b/tj" || exit 1
"$b/tj" || fail=1
exit ${fail:-0}
