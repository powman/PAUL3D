#!/usr/bin/env bash
# Puts PAUL3D on the console over FTP (2121): the title from build/title (scripts/build-mg.sh). Nothing is deleted.
# The text model comes from the page (Modelos), or copy a .gguf to /data/PAUL3D/models/text yourself.
set -u
H=${PS5_HOST:?set PS5_HOST to the console address}
cd "$(dirname "$0")/.." || exit 1
put() { # local remote
  start=$(date +%s)
  curl -sS --ftp-create-dirs -T "$1" "ftp://$H:2121$2" && echo "ok  $2 ($(stat -c %s "$1") bytes, $(( $(date +%s) - start )) s)" || echo "FAIL $2"
}
cd build/title || exit 1
find PPSA99577 -type f | while read -r f; do put "$f" "/data/homebrew/$f"; done
echo UPLOAD-DONE
