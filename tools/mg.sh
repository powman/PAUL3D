#!/usr/bin/env bash
# Drives PAUL3D on the console (FTP 2121, elfldr 9021).
#   mg.sh put <local> <remote-under-/data/PAUL3D>   upload a file
#   mg.sh log                                      the console's app.log
#   mg.sh alive                                    title running? (log changed in the last 3 minutes and no 'worker ends')
#   mg.sh launch                                   start the title (launch payload for PPSA99577)
#   mg.sh quit                                     ask the title to end itself
#   mg.sh setkey <key>                             protect the web interface and API with a key (empty: open again)
set -u
H=${PS5_HOST:?set PS5_HOST to the console address}
F=ftp://$H:2121/data/PAUL3D
here=$(cd "$(dirname "$0")/.." && pwd)
case ${1:-} in
  put) curl -sS --ftp-create-dirs -T "$2" "$F/$3" ;;
  log) curl -sS "$F/app.log" ;;
  alive)
    log=$(curl -sS "$F/app.log"); echo "$log" | tail -1 | cut -c1-120
    echo "$log" | grep -q "worker ends" && echo "ENDED" || echo "no end marker" ;;
  launch) PS5_HOST=$H PS5_PORT=9021 bash "$here/third_party/PS5SD/scripts/send-elf.sh" "${MG_LAUNCH_ELF:-$here/build/tools/launch-mg.elf}" ;;
  quit) : > /tmp/mg-quit; curl -sS -T /tmp/mg-quit "$F/quit" && echo "quit sent" ;;
  setkey) echo "${2:-}" > /tmp/mg-key; curl -sS --ftp-create-dirs -T /tmp/mg-key "$F/api-key.txt" && echo "key set; the title reads it on every request" ;;
  *) sed -n 2,12p "$0" ;;
esac
