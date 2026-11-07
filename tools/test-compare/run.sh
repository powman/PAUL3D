#!/usr/bin/env bash
# Checks tools/compare-glb.py on small GLB files made here: same cube, a bigger cube, two meshes (one in a moved node), not a GLB.
#   bash tools/test-compare/run.sh
set -u
here=$(cd "$(dirname "$0")" && pwd); cmp="$here/../compare-glb.py"
d=$(mktemp -d); trap 'rm -rf "$d"' EXIT
python3 - "$d" <<'PY'
import json, struct, sys
d = sys.argv[1]
V = [(x, y, z) for x in (0, 1) for y in (0, 1) for z in (0, 1)]
F = [(0,1,3),(0,3,2),(4,6,7),(4,7,5),(0,4,5),(0,5,1),(2,3,7),(2,7,6),(0,2,6),(0,6,4),(1,5,7),(1,7,3)]
def glb(path, meshes, nodes):
    binary = b""; acc = []; views = []; jm = []
    for s in meshes:
        pos = b"".join(struct.pack("<3f", *(c * s for c in v)) for v in V)
        idx = b"".join(struct.pack("<3H", *f) for f in F)
        for data, target in ((pos, 34962), (idx, 34963)):
            views.append({"buffer": 0, "byteOffset": len(binary), "byteLength": len(data), "target": target})
            binary += data + b"\0" * (-len(data) % 4)
        n = len(views)
        acc.append({"bufferView": n - 2, "componentType": 5126, "count": 8, "type": "VEC3", "min": [0, 0, 0], "max": [s, s, s]})
        acc.append({"bufferView": n - 1, "componentType": 5123, "count": 36, "type": "SCALAR"})
        jm.append({"primitives": [{"attributes": {"POSITION": len(acc) - 2}, "indices": len(acc) - 1}]})
    j = {"asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": list(range(len(nodes)))}], "nodes": nodes,
         "meshes": jm, "accessors": acc, "bufferViews": views, "buffers": [{"byteLength": len(binary)}]}
    js = json.dumps(j).encode(); js += b" " * (-len(js) % 4)
    out = struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(js) + 8 + len(binary))
    out += struct.pack("<I4s", len(js), b"JSON") + js + struct.pack("<I4s", len(binary), b"BIN\0") + binary
    open(f"{d}/{path}", "wb").write(out)
glb("cube.glb", [1], [{"mesh": 0}])
glb("cube2.glb", [1], [{"mesh": 0}])
glb("cube_big.glb", [1.05], [{"mesh": 0}])
glb("two.glb", [1, 1], [{"mesh": 0}, {"mesh": 1, "translation": [2, 0, 0]}])
open(f"{d}/text.glb", "w").write("not a glb")
PY
fail=0
t() { if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: expected [$2], got [$3]"; fail=1; fi; }
run() { out=$(python3 "$cmp" "$@" 2>&1); rc=$?; }
run "$d/cube.glb" "$d/cube2.glb";    t "same cube passes (exit)" 0 "$rc"; t "same cube says PASS" 1 "$(grep -c '^PASS' <<<"$out")"
run "$d/cube.glb" "$d/cube_big.glb"; t "5 % bigger fails (exit)" 1 "$rc"; t "5 % bigger names the bbox" 1 "$(grep -c '^FAIL: bbox' <<<"$out")"
run "$d/cube.glb" "$d/cube_big.glb" --tol 0.06; t "5 % bigger passes with --tol 0.06" 0 "$rc"
run "$d/cube.glb" "$d/two.glb";      t "two meshes: faces differ (exit)" 1 "$rc"; t "two meshes: faces 12 vs 24" 1 "$(grep -c '^FAIL: faces 12 vs 24' <<<"$out")"
run "$d/two.glb" "$d/two.glb";       t "node translation in the bbox (x = 3)" 1 "$(grep -Ec 'bbox A .*3(\.0+)? x' <<<"$out")"
run "$d/cube.glb" "$d/text.glb";     t "not a GLB (exit 2)" 2 "$rc"
exit $fail
