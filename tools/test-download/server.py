"""A file server for tools/test-download: Range requests, and two faults on demand.
  GET /<mode>/file.bin   mode ok: the file; cut: stops after half of what was asked (the first time only); bad: wrong bytes
Every request's Range header is appended to <log>.   usage: server.py <port> <file> <log>"""
import http.server, os, sys
port, path, log = int(sys.argv[1]), sys.argv[2], sys.argv[3]
data = open(path, "rb").read()
cut_done = set()
class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        mode = self.path.split("/")[1]
        rng = self.headers.get("Range", "")
        open(log, "a").write(f"{self.path} {rng or '-'}\n")
        start = int(rng.split("=")[1].split("-")[0]) if rng else 0
        body = data[start:] if mode != "bad" else bytes(len(data) - start)
        self.send_response(206 if rng else 200)
        self.send_header("Content-Length", str(len(body)))
        if rng: self.send_header("Content-Range", f"bytes {start}-{len(data) - 1}/{len(data)}")
        self.end_headers()
        if mode == "cut" and self.path not in cut_done:
            cut_done.add(self.path)
            self.wfile.write(body[: len(body) // 2]); self.wfile.flush()
            self.connection.shutdown(2); self.close_connection = True; return
        self.wfile.write(body)
    def log_message(self, *a): pass
    def handle(self):
        try: super().handle()
        except OSError: pass  # the cut connection
http.server.ThreadingHTTPServer(("127.0.0.1", port), H).serve_forever()
