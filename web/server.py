#!/usr/bin/env python3
from __future__ import annotations

import json
import os
import tempfile
import time
import uuid
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path


ROOT = Path(__file__).resolve().parent
DATA_DIR = Path(os.environ.get("ESPMARK_DATA_DIR", "/data"))
RESULTS_FILE = DATA_DIR / "results.json"
MAX_BODY = 256 * 1024


class EspmarkHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def do_GET(self):
        if self.path == "/api/health":
            self.send_json({"status": "ok"})
            return
        if self.path == "/api/results":
            self.send_json(load_results())
            return
        super().do_GET()

    def do_POST(self):
        if self.path != "/api/results":
            self.send_error(404)
            return

        length = int(self.headers.get("Content-Length", "0"))
        if length <= 0 or length > MAX_BODY:
            self.send_json({"error": "invalid request size"}, status=400)
            return

        try:
            payload = json.loads(self.rfile.read(length).decode("utf-8"))
            record = validate_submission(payload)
        except ValueError as error:
            self.send_json({"error": str(error)}, status=400)
            return

        results = load_results()
        results.insert(0, record)
        save_results(results[:500])
        self.send_json({"status": "saved", "id": record["id"]}, status=201)

    def send_json(self, data, status=200):
        body = json.dumps(data, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def load_results():
    if not RESULTS_FILE.exists():
        return []
    try:
        return json.loads(RESULTS_FILE.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return []


def save_results(results):
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    fd, tmp_name = tempfile.mkstemp(prefix="results-", suffix=".json", dir=DATA_DIR)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as handle:
            json.dump(results, handle, ensure_ascii=False, indent=2)
            handle.write("\n")
        os.replace(tmp_name, RESULTS_FILE)
    finally:
        if os.path.exists(tmp_name):
            os.unlink(tmp_name)


def validate_submission(payload):
    contributor = str(payload.get("contributor", "")).strip()
    if not (2 <= len(contributor) <= 80):
        raise ValueError("invalid contributor name")

    if str(payload.get("bot_field", "")).strip():
        raise ValueError("bot check failed")

    if int(payload.get("form_elapsed_ms", 0)) < 3000:
        raise ValueError("submission was too fast")

    result = payload.get("result")
    if not isinstance(result, dict):
        raise ValueError("missing result")

    board = result.get("board")
    config = result.get("config")
    metrics = result.get("results")
    if not isinstance(board, dict) or not isinstance(config, dict) or not isinstance(metrics, list):
        raise ValueError("invalid result structure")

    required_tests = {
        "cpu.integer.add_mul.u32",
        "cpu.integer.div_mod.u32",
        "cpu.integer.branch.u32",
        "cpu.integer.crc_like.u32",
    }
    submitted_tests = {item.get("test_id") for item in metrics if isinstance(item, dict)}
    if not required_tests.issubset(submitted_tests):
        raise ValueError("missing benchmark metrics")

    return {
        "id": uuid.uuid4().hex,
        "submitted_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "contributor": contributor,
        "result": result,
    }


def main():
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    server = ThreadingHTTPServer(("0.0.0.0", 8000), EspmarkHandler)
    print("espmark web server listening on :8000", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()

