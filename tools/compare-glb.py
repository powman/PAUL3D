"""Compares two GLB meshes: face count and bounding box (all meshes of the scene, node transforms applied).
usage: python3 compare-glb.py A.glb B.glb [--tol 0.01]
Prints the faces and the box of both, then PASS or FAIL: <reason>. Exit 0 PASS, 1 FAIL, 2 not a readable GLB.
Used to check that a change to the engine (a new ggml) gives the same mesh for the same picture and seed."""
import json
import math
import struct
import sys

COMPONENTS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}
FORMATS = {5120: "b", 5121: "B", 5122: "h", 5123: "H", 5125: "I", 5126: "f"}


def mat_mul(a, b):
    return [sum(a[r * 4 + k] * b[k * 4 + c] for k in range(4)) for r in range(4) for c in range(4)]


def node_matrix(node):
    """Row-major 4x4 of a glTF node (its matrix is column-major; TRS otherwise)."""
    if "matrix" in node:
        m = node["matrix"]
        return [m[c * 4 + r] for r in range(4) for c in range(4)]
    tx, ty, tz = node.get("translation", (0, 0, 0))
    qx, qy, qz, qw = node.get("rotation", (0, 0, 0, 1))
    sx, sy, sz = node.get("scale", (1, 1, 1))
    r = [1 - 2 * (qy * qy + qz * qz), 2 * (qx * qy - qz * qw), 2 * (qx * qz + qy * qw),
         2 * (qx * qy + qz * qw), 1 - 2 * (qx * qx + qz * qz), 2 * (qy * qz - qx * qw),
         2 * (qx * qz - qy * qw), 2 * (qy * qz + qx * qw), 1 - 2 * (qx * qx + qy * qy)]
    return [r[0] * sx, r[1] * sy, r[2] * sz, tx, r[3] * sx, r[4] * sy, r[5] * sz, ty,
            r[6] * sx, r[7] * sy, r[8] * sz, tz, 0, 0, 0, 1]


def read(path):
    """(faces, (min xyz), (max xyz)) of every mesh in the default scene."""
    data = open(path, "rb").read()
    magic, _, _ = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF":
        raise ValueError("not a GLB")
    jlen, _ = struct.unpack_from("<I4s", data, 12)
    j = json.loads(data[20:20 + jlen])
    binary = data[20 + jlen + 8:]

    def accessor(i):
        a = j["accessors"][i]
        v = j["bufferViews"][a["bufferView"]]
        n, fmt = COMPONENTS[a["type"]], FORMATS[a["componentType"]]
        size = struct.calcsize("<" + fmt)
        stride = v.get("byteStride") or size * n
        start = v.get("byteOffset", 0) + a.get("byteOffset", 0)
        return [struct.unpack_from("<" + fmt * n, binary, start + k * stride) for k in range(a["count"])]

    faces = 0
    lo, hi = [math.inf] * 3, [-math.inf] * 3
    identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]

    def walk(ni, parent):
        nonlocal faces
        node = j["nodes"][ni]
        m = mat_mul(parent, node_matrix(node))
        if "mesh" in node:
            for p in j["meshes"][node["mesh"]]["primitives"]:
                count = j["accessors"][p["indices"]]["count"] if "indices" in p else j["accessors"][p["attributes"]["POSITION"]]["count"]
                faces += count // 3
                for x, y, z in accessor(p["attributes"]["POSITION"]):
                    w = (m[0] * x + m[1] * y + m[2] * z + m[3], m[4] * x + m[5] * y + m[6] * z + m[7],
                         m[8] * x + m[9] * y + m[10] * z + m[11])
                    for k in range(3):
                        lo[k] = min(lo[k], w[k])
                        hi[k] = max(hi[k], w[k])
        for c in node.get("children", []):
            walk(c, m)

    for ni in j["scenes"][j.get("scene", 0)]["nodes"]:
        walk(ni, identity)
    return faces, lo, hi


def main(argv):
    args = [a for a in argv if not a.startswith("--")]
    tol = float(argv[argv.index("--tol") + 1]) if "--tol" in argv else 0.01
    if "--tol" in argv:
        args.remove(argv[argv.index("--tol") + 1])
    if len(args) != 2:
        print(__doc__.strip())
        return 2
    try:
        (fa, loa, hia), (fb, lob, hib) = read(args[0]), read(args[1])
    except (OSError, ValueError, KeyError, IndexError, struct.error, json.JSONDecodeError) as e:
        print(f"cannot read: {e}")
        return 2
    sa = [h - l for l, h in zip(loa, hia)]
    sb = [h - l for l, h in zip(lob, hib)]
    fmt = lambda s: " x ".join(f"{v:.4f}" for v in s)
    print(f"faces A {fa}  B {fb}")
    print(f"bbox A {fmt(sa)}  B {fmt(sb)}")
    if abs(fa - fb) > tol * max(fa, fb, 1):
        print(f"FAIL: faces {fa} vs {fb}")
        return 1
    for k in range(3):
        if abs(sa[k] - sb[k]) > tol * max(sa[k], sb[k], 1e-9):
            print(f"FAIL: bbox axis {'xyz'[k]} {sa[k]:.4f} vs {sb[k]:.4f}")
            return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
