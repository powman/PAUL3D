#!/usr/bin/env bash
# Puts PAUL3D on the console over FTP (2121): the title, the 512-path models, a test image. Nothing is deleted.
set -u
H=${PS5_HOST:?set PS5_HOST to the console address}
cd "$(dirname "$0")/.." || exit 1
put() { # local remote
  start=$(date +%s)
  curl -sS --ftp-create-dirs -T "$1" "ftp://$H:2121$2" && echo "ok  $2 ($(stat -c %s "$1") bytes, $(( $(date +%s) - start )) s)" || echo "FAIL $2"
}
for f in build-ps5/PPSA99577/eboot.bin build-ps5/PPSA99577/sce_module/libc.prx build-ps5/PPSA99577/sce_sys/*; do
  put "$f" "/data/homebrew/PPSA99577/${f#build-ps5/PPSA99577/}"
done
put samples/sword.png /data/PAUL3D/sword.png
for f in models/q4/*.gguf; do
  case $f in *_1024.gguf) continue ;; esac
  put "$f" "/data/PAUL3D/models/q4/$(basename "$f")"
done
echo "/data/PAUL3D/sword.png /data/PAUL3D/sword.glb -m /data/PAUL3D/models/q4 --res 512 -s 1234 --fa-fast --steps 8 --box-uv --no-texture -v" > /tmp/mg-args.txt
put /tmp/mg-args.txt /data/PAUL3D/args.txt
echo UPLOAD-DONE
