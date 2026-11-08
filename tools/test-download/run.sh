#!/usr/bin/env bash
# Builds tools/test-download/test.cpp with ps5/mg_download.cpp and runs it against server.py (Range, a cut, wrong bytes).
#   bash tools/test-download/run.sh      (cpp-httplib: ps5/third_party)
set -u
root=$(cd "$(dirname "$0")/../.." && pwd); b=$(mktemp -d)
httplib=${HTTPLIB_DIR:-$root/ps5/third_party/cpp-httplib}
head -c 3000000 /dev/urandom > "$b/file.bin"; sha=$(sha256sum "$b/file.bin" | cut -d' ' -f1)
port=$((20000 + RANDOM % 20000))
python3 "$root/tools/test-download/server.py" $port "$b/file.bin" "$b/log" & srv=$!
trap 'kill $srv 2>/dev/null; rm -rf "$b"' EXIT
g++ -std=c++17 -O1 -Wall -I "$root/ps5" -I "$httplib" "$root/tools/test-download/test.cpp" "$root/ps5/mg_download.cpp" -o "$b/t" -lpthread || exit 1
sleep 0.5
"$b/t" "$root" $port "$b/file.bin" "$sha" "$b/log"
