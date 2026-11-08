"""A PAUL3D stand-in for checking ps5/index.html (Modelos) and ps5/cad.html (Peça por texto) in a browser, on port 7870.
No text model at first; a download of a group takes about 6 s (progress, cancel); once the text group is installed,
/api/text answers with a key-ring OpenSCAD program, first partly, then whole.
  python3 tools/test-cad/mock-paul3d.py      then open http://127.0.0.1:7870/ and http://127.0.0.1:7870/cad"""
import http.server, json, os, threading, time

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "ps5")
GROUPS = {"mesh-q4": ("Modelos 3D (TRELLIS.2, q4)", 6105943648, "3d", "https://huggingface.co/ilintar/trellis2-gguf"),
          "text-qwen35-9b": ("IA de texto (Qwen3.5 9B)", 5680522464, "text", "https://huggingface.co/unsloth/Qwen3.5-9B-GGUF")}
installed = {"mesh-q4": True, "text-qwen35-9b": False}
downloads, texts = {}, {}
ANSWER = ("```openscad\n// chaveiro\nl=50; w=20; h=3;\ndifference(){\n  cube([l,w,h]);\n  translate([6,w/2,-0.5]) cylinder(d=5,h=h+1,$fn=64);\n}\n"
          "translate([13,5,h-0.01]) linear_extrude(1) text(\"PAUL\",size=9);\n```\nChaveiro com furo de 5 mm.")
lock = threading.Lock()


def tick():
    while True:
        time.sleep(0.5)
        with lock:
            for d in downloads.values():
                if d["state"] in ("queued", "downloading"):
                    d["state"] = "downloading"
                    d["done"] = min(d["total"], d["done"] + d["total"] // 12)
                    d["speed"] = 60e6
                    if d["done"] >= d["total"]:
                        d["state"], d["speed"] = "done", 0
                        installed[d["group"]] = True


class H(http.server.BaseHTTPRequestHandler):
    def send(self, code, obj, ctype="application/json"):
        body = obj if isinstance(obj, bytes) else json.dumps(obj).encode()
        self.send_response(code); self.send_header("Content-Type", ctype); self.send_header("Content-Length", str(len(body)))
        self.end_headers(); self.wfile.write(body)

    def do_GET(self):
        p = self.path.split("?")[0]
        if p in ("/", "/index.html"): return self.send(200, open(os.path.join(ROOT, "index.html"), "rb").read(), "text/html")
        if p == "/cad": return self.send(200, open(os.path.join(ROOT, "cad.html"), "rb").read(), "text/html")
        if p == "/api/status": return self.send(200, {"busy": False, "stage": "", "progress": "", "pool_free_gib": 10.8, "pool_gib": 11.4, "threads": 11, "tail": []})
        if p == "/api/jobs": return self.send(200, [])
        if p == "/api/models": return self.send(200, [{"dir": "q4", "res1024": True, "pixal3d": False}])
        if p == "/api/weights": return self.send(200, {"files": [], "complete": True})
        if p == "/api/auth": return self.send(200, {"required": False, "ok": True})
        if p == "/api/catalog":
            with lock:
                tm = ["Qwen3.8-27B-UD-Q2_K_XL.gguf"] + (["Qwen3.5-9B-Q4_K_M.gguf"] if installed["text-qwen35-9b"] else [])
                return self.send(200, {"free": 120e9, "text_models": sorted(tm), "text_default": sorted(tm)[0], "groups": [{"group": g, "title": t, "size": s, "installed": installed[g],
                    "have": s if installed[g] else next((d["done"] for d in downloads.values() if d["group"] == g), 0),
                    "licence": l, "needed_for": u} for g, (t, s, u, l) in GROUPS.items()]})
        if p == "/api/downloads":
            with lock: return self.send(200, sorted(downloads.values(), key=lambda d: d["id"], reverse=True))
        if p == "/v1/models":
            ids = sorted(["Qwen3.8-27B-UD-Q2_K_XL.gguf"] + (["Qwen3.5-9B-Q4_K_M.gguf"] if installed["text-qwen35-9b"] else []))
            return self.send(200, {"object": "list", "data": [{"id": i, "object": "model"} for i in ids]})
        if p.startswith("/api/text/"):
            t = texts.get(p.rsplit("/", 1)[1])
            if not t: return self.send(404, {"error": "no such text request"})
            n = t["polls"] = t["polls"] + 1
            if n < 2: return self.send(200, {"id": t["id"], "status": "loading", "text": ""})
            if n < 3: return self.send(200, {"id": t["id"], "status": "generating", "text": ANSWER[:60]})
            return self.send(200, {"id": t["id"], "status": "done", "text": ANSWER})
        self.send(404, {"error": "not found"})

    def do_POST(self):
        p = self.path
        body = self.rfile.read(int(self.headers.get("Content-Length", 0) or 0))
        if p == "/api/downloads":
            j = json.loads(body or b"{}")
            if j.get("accept_license") is not True: return self.send(400, {"error": "accept_license must be true"})
            if j.get("group") not in GROUPS: return self.send(404, {"error": "no such group"})
            with lock:
                did = "d%d" % (len(downloads) + 1)
                downloads[did] = {"id": did, "group": j["group"], "title": GROUPS[j["group"]][0], "state": "queued", "done": 0,
                                  "total": GROUPS[j["group"]][1], "speed": 0}
            return self.send(202, {"id": did})
        if p.startswith("/api/downloads/") and p.endswith("/cancel"):
            with lock:
                d = downloads.get(p.split("/")[3])
                if not d: return self.send(404, {"error": "no such download"})
                d["state"] = "cancelled"
            return self.send(200, {"ok": True})
        if p == "/api/text":
            open(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".last-text-request.json"), "wb").write(body)
            tid = "t%d" % (len(texts) + 1)
            texts[tid] = {"id": tid, "polls": 0}
            return self.send(202, {"id": tid})
        self.send(404, {"error": "not found"})

    def log_message(self, *a): pass


threading.Thread(target=tick, daemon=True).start()
http.server.ThreadingHTTPServer(("127.0.0.1", 7870), H).serve_forever()
