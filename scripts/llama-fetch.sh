#!/usr/bin/env bash
# llama.cpp for PAUL3D's text engine: the commit whose ggml is 0.27.0 (the unified ggml, scripts/ggml-unified.sh), with
# ps5/patch-llama-io.py applied, and one missing include added (src/models/clef.cpp uses std::find without <algorithm>;
# GCC 16 and the console's clang/libc++ do not bring it in through other headers). Only src/, include/ and vendor/ are built from it, never its own ggml/.
#   scripts/llama-fetch.sh <dest>
# <dest>/.paul3d-llama records the commit; with it there, nothing is done.
set -euo pipefail
dest=${1:?usage: llama-fetch.sh <dest>}
LLAMA=4ed1c8507e78c5dddc8e48d2822b87c2099e00d1   # ggml-org/llama.cpp "ggml : bump version to 0.27.0", 2026-10-15
here=$(cd "$(dirname "$0")/.." && pwd)
MARK="llama $LLAMA +io +algorithm"
if [ -f "$dest/.paul3d-llama" ] && [ "$(cat "$dest/.paul3d-llama")" = "$MARK" ]; then
    echo "llama-fetch: $dest is already there"
    exit 0
fi
rm -rf "$dest"
git init -q "$dest"
git -C "$dest" fetch -q --depth 1 https://github.com/ggml-org/llama.cpp "$LLAMA"
git -C "$dest" -c advice.detachedHead=false checkout -q FETCH_HEAD
python3 "$here/ps5/patch-llama-io.py" "$dest"
grep -q '#include <algorithm>' "$dest/src/models/clef.cpp" || sed -i '1i #include <algorithm>' "$dest/src/models/clef.cpp"
echo "$MARK" > "$dest/.paul3d-llama"
echo "llama-fetch: $dest = llama.cpp ${LLAMA:0:10} + patch-llama-io"
