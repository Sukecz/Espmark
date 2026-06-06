#!/usr/bin/env python3
from __future__ import annotations

import json
import math
import os
import base64
import binascii
import hashlib
import hmac
import secrets
import sqlite3
import time
import uuid
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
from urllib import error as urlerror
from urllib import request as urlrequest


ROOT = Path(__file__).resolve().parent
DATA_DIR = Path(os.environ.get("ESPMARK_DATA_DIR", "/data"))
RESULTS_FILE = DATA_DIR / "results.json"
DATABASE_FILE = DATA_DIR / "espmark.sqlite3"
BOARDS_FILE = ROOT / "boards.json"
SCORING_FILE = ROOT / "scoring" / "espmark-score-preview-3.json"
MAX_BODY = 256 * 1024
CHALLENGE_TTL_SECONDS = 10 * 60
CHALLENGE_SECRET = os.environ.get("ESPMARK_CHALLENGE_SECRET") or secrets.token_hex(32)
USED_CHALLENGE_NONCES = set()
SUBMISSION_SCHEMA_VERSION = "1.0.0-beta"
SCORING_VERSION = "espmark-score-preview-3"
REFERENCE_SET_ID = "espmark-ref-stress-preview-1"
BENCHMARK_PROFILE = "espmark-core-stress-preview"
TEST_SET_ID = "espmark-core-stress-preview-1"
UMAMI_BASE_URL = os.environ.get("UMAMI_BASE_URL", "http://192.168.1.4:8096").rstrip("/")
UMAMI_TIMEOUT_SECONDS = 5


SCORING_METRICS = {
    "cpu.integer.add_mul.u32": {
        "category": "cpu",
        "subcategory": "CPU integer micro-suite",
        "label": "Basic math",
        "reference": 10000.0,
        "direction": "lower_is_better",
        "weight": 4.5,
        "required": True,
    },
    "cpu.integer.div_mod.u32": {
        "category": "cpu",
        "subcategory": "CPU integer micro-suite",
        "label": "Hard math",
        "reference": 30000.0,
        "direction": "lower_is_better",
        "weight": 4.5,
        "required": True,
    },
    "cpu.integer.branch.u32": {
        "category": "cpu",
        "subcategory": "CPU integer micro-suite",
        "label": "Decision speed",
        "reference": 12000.0,
        "direction": "lower_is_better",
        "weight": 4.5,
        "required": True,
    },
    "cpu.integer.crc_like.u32": {
        "category": "cpu",
        "subcategory": "CPU integer micro-suite",
        "label": "Data crunching",
        "reference": 45000.0,
        "direction": "lower_is_better",
        "weight": 4.5,
        "required": True,
    },
    "memory.ram.memcpy.seq": {
        "category": "memory",
        "subcategory": "Memory/RAM bandwidth",
        "label": "RAM copy",
        "reference": 16000.0,
        "direction": "lower_is_better",
        "weight": 6.67,
        "required": True,
    },
    "memory.ram.memset.seq": {
        "category": "memory",
        "subcategory": "Memory/RAM bandwidth",
        "label": "RAM fill",
        "reference": 16000.0,
        "direction": "lower_is_better",
        "weight": 6.67,
        "required": True,
    },
    "memory.ram.read.strided": {
        "category": "memory",
        "subcategory": "Memory/RAM bandwidth",
        "label": "RAM read",
        "reference": 16000.0,
        "direction": "lower_is_better",
        "weight": 6.67,
        "required": True,
    },
    "memory.heap.malloc_free.128b": {
        "category": "memory",
        "subcategory": "Heap alloc/free churn",
        "label": "Small allocations",
        "reference": 12000.0,
        "direction": "lower_is_better",
        "weight": 6,
        "required": True,
    },
    "memory.heap.fragmentation": {
        "category": "memory",
        "subcategory": "Heap fragmentation",
        "label": "Heap fragmentation",
        "reference": 6000.0,
        "direction": "lower_is_better",
        "weight": 4,
        "required": True,
    },
    "cpu.sustained.mix": {
        "category": "cpu",
        "subcategory": "CPU sustained stress",
        "label": "Sustained CPU mix",
        "reference": 35000.0,
        "direction": "lower_is_better",
        "weight": 8,
        "required": True,
    },
    "cpu.mandelbrot.q16": {
        "category": "cpu",
        "subcategory": "Deterministic compute test",
        "label": "Mandelbrot fixed-point",
        "reference": 40000.0,
        "direction": "lower_is_better",
        "weight": 8,
        "required": True,
    },
    "cpu.matrix.i16": {
        "category": "cpu",
        "subcategory": "Supplemental compute",
        "label": "Matrix multiply",
        "reference": 30000.0,
        "direction": "lower_is_better",
        "weight": 0,
        "required": False,
    },
    "cpu.float32.affine": {
        "category": "cpu",
        "subcategory": "CPU float/math",
        "label": "Float32 affine",
        "reference": 25000.0,
        "direction": "lower_is_better",
        "weight": 6,
        "required": True,
    },
    "flash.read.seq": {
        "category": "flash",
        "subcategory": "Flash read",
        "label": "Flash sequential read",
        "reference": 128000.0,
        "direction": "lower_is_better",
        "weight": 10,
        "required": True,
    },
    "practical.json.roundtrip": {
        "category": "practical_iot",
        "subcategory": "JSON parse/generate",
        "label": "JSON roundtrip",
        "reference": 50000.0,
        "direction": "lower_is_better",
        "weight": 7,
        "required": True,
    },
    "practical.string.format": {
        "category": "practical_iot",
        "subcategory": "String formatting",
        "label": "String formatting",
        "reference": 25000.0,
        "direction": "lower_is_better",
        "weight": 3,
        "required": True,
    },
    "practical.crc32.sw": {
        "category": "practical_iot",
        "subcategory": "CRC32 software",
        "label": "CRC32 software",
        "reference": 35000.0,
        "direction": "lower_is_better",
        "weight": 3,
        "required": True,
    },
    "practical.sha256.sw": {
        "category": "practical_iot",
        "subcategory": "SHA-256 software",
        "label": "SHA-256 software",
        "reference": 70000.0,
        "direction": "lower_is_better",
        "weight": 7,
        "required": True,
    },
}

CATEGORY_WEIGHTS = {
    "cpu": 40,
    "memory": 30,
    "flash": 10,
    "practical_iot": 20,
}


def load_scoring_config():
    with SCORING_FILE.open(encoding="utf-8") as file:
        config = json.load(file)
    if not isinstance(config.get("metrics"), dict) or not config["metrics"]:
        raise RuntimeError("scoring registry has no metrics")
    return config


SCORING_CONFIG = load_scoring_config()
SUBMISSION_SCHEMA_VERSION = SCORING_CONFIG["submission_schema_version"]
SCORING_VERSION = SCORING_CONFIG["scoring_version"]
REFERENCE_SET_ID = SCORING_CONFIG["reference_set_id"]
BENCHMARK_PROFILE = SCORING_CONFIG["benchmark_profile"]
TEST_SET_ID = SCORING_CONFIG["test_set_id"]
SCORING_METRICS = SCORING_CONFIG["metrics"]
CATEGORY_WEIGHTS = SCORING_CONFIG["category_weights"]


class EspmarkHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def do_GET(self):
        if self.path == "/script":
            self.proxy_umami("/script")
            return
        if self.path == "/api/health":
            self.send_json({"status": "ok"})
            return
        if self.path == "/api/challenge":
            self.send_json(create_challenge())
            return
        if self.path == "/api/results":
            self.send_json(load_results(limit=500))
            return
        if self.path == "/api/scoring":
            self.send_json(public_scoring_config())
            return
        super().do_GET()

    def do_POST(self):
        if self.path == "/api/send":
            self.proxy_umami("/api/send", method="POST")
            return
        if self.path == "/api/score-preview":
            self.handle_score_preview()
            return
        if self.path == "/api/bug-reports":
            self.handle_bug_report()
            return
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

        insert_submission(record)
        self.send_json({"status": "saved", "id": record["id"], "submission": record}, status=201)

    def do_HEAD(self):
        if self.path == "/script":
            self.proxy_umami("/script", head_only=True)
            return
        super().do_HEAD()

    def handle_score_preview(self):
        length = int(self.headers.get("Content-Length", "0"))
        if length <= 0 or length > MAX_BODY:
            self.send_json({"error": "invalid request size"}, status=400)
            return

        try:
            payload = json.loads(self.rfile.read(length).decode("utf-8"))
            raw_result = payload.get("result")
            if not isinstance(raw_result, dict):
                raise ValueError("missing result")
            record = build_submission_record(
                raw_result,
                contributor="Preview",
                transport={"kind": "webserial"},
                validate_board=False,
                allow_incomplete=True,
                publication_status="preview",
                public=False,
            )
        except ValueError as error:
            self.send_json({"error": str(error)}, status=400)
            return

        self.send_json({"status": "ok", "submission": record})

    def handle_bug_report(self):
        length = int(self.headers.get("Content-Length", "0"))
        if length <= 0 or length > 16 * 1024:
            self.send_json({"error": "invalid request size"}, status=400)
            return

        try:
            payload = json.loads(self.rfile.read(length).decode("utf-8"))
            record = validate_bug_report(payload)
        except ValueError as error:
            self.send_json({"error": str(error)}, status=400)
            return

        insert_bug_report(record)
        self.send_json({"status": "saved", "id": record["id"]}, status=201)

    def send_json(self, data, status=200):
        body = json.dumps(data, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def proxy_umami(self, target_path, method="GET", head_only=False):
        body = None
        if method == "POST":
            length = int(self.headers.get("Content-Length", "0"))
            if length > 16 * 1024:
                self.send_error(413)
                return
            body = self.rfile.read(length) if length else b""
            if target_path == "/api/send":
                body = normalize_umami_payload(body)

        headers = {
            "User-Agent": self.headers.get("User-Agent", ""),
            "Referer": self.headers.get("Referer", ""),
            "Content-Type": self.headers.get("Content-Type", ""),
            "X-Forwarded-For": self.client_address[0],
            "X-Forwarded-Proto": "https" if self.headers.get("X-Forwarded-Proto") == "https" else "http",
            "X-Forwarded-Host": self.headers.get("Host", ""),
        }
        request = urlrequest.Request(
            f"{UMAMI_BASE_URL}{target_path}",
            data=body,
            headers={key: value for key, value in headers.items() if value},
            method=method,
        )

        try:
            with urlrequest.urlopen(request, timeout=UMAMI_TIMEOUT_SECONDS) as response:
                response_body = response.read()
                self.send_response(response.status)
                content_type = response.headers.get("Content-Type")
                if content_type:
                    self.send_header("Content-Type", content_type)
                if target_path == "/script":
                    self.send_header("Cache-Control", "public, max-age=3600")
                else:
                    self.send_header("Cache-Control", "no-store")
                self.send_header("Content-Length", str(len(response_body)))
                self.end_headers()
                if not head_only:
                    self.wfile.write(response_body)
        except urlerror.HTTPError as error:
            response_body = error.read()
            self.send_response(error.code)
            content_type = error.headers.get("Content-Type")
            if content_type:
                self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(response_body)))
            self.end_headers()
            if not head_only:
                self.wfile.write(response_body)
        except OSError:
            if target_path == "/script":
                self.send_error(502)
            else:
                self.send_response(204)
                self.send_header("Cache-Control", "no-store")
                self.send_header("Content-Length", "0")
                self.end_headers()


def db_connect():
    connection = sqlite3.connect(DATABASE_FILE)
    connection.row_factory = sqlite3.Row
    return connection


def normalize_umami_payload(body):
    try:
        payload = json.loads(body.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        return body
    if not isinstance(payload, dict):
        return body
    event_payload = payload.get("payload")
    if isinstance(event_payload, dict) and "data" not in event_payload:
        event_payload["data"] = {}
        return json.dumps(payload, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    return body


def init_database():
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    with db_connect() as connection:
        connection.executescript(
            """
            PRAGMA journal_mode = WAL;
            PRAGMA foreign_keys = ON;

            CREATE TABLE IF NOT EXISTS scoring_versions (
                id TEXT PRIMARY KEY,
                reference_set_id TEXT NOT NULL,
                config_json TEXT NOT NULL,
                active INTEGER NOT NULL DEFAULT 1,
                created_at TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS reference_sets (
                id TEXT PRIMARY KEY,
                metrics_json TEXT NOT NULL,
                created_at TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS submissions (
                id TEXT PRIMARY KEY,
                submitted_at TEXT NOT NULL,
                contributor TEXT NOT NULL,
                board_selected_by_user TEXT NOT NULL,
                board_label_trusted INTEGER NOT NULL DEFAULT 0,
                mode TEXT NOT NULL,
                benchmark_profile TEXT NOT NULL,
                test_set_id TEXT NOT NULL,
                scoring_version TEXT NOT NULL,
                reference_set_id TEXT NOT NULL,
                publication_status TEXT NOT NULL DEFAULT 'published',
                public INTEGER NOT NULL DEFAULT 1,
                transport_json TEXT NOT NULL,
                validation_json TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS raw_results (
                submission_id TEXT PRIMARY KEY REFERENCES submissions(id) ON DELETE CASCADE,
                schema_version TEXT,
                firmware_version TEXT,
                target_family TEXT,
                raw_json TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS computed_scores (
                submission_id TEXT PRIMARY KEY REFERENCES submissions(id) ON DELETE CASCADE,
                scoring_version TEXT NOT NULL,
                reference_set_id TEXT NOT NULL,
                scores_json TEXT NOT NULL,
                metric_scores_json TEXT NOT NULL,
                normalization_json TEXT NOT NULL,
                computed_at TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS firmware_builds (
                id TEXT PRIMARY KEY,
                firmware_version TEXT NOT NULL,
                target_family TEXT NOT NULL,
                build_hash TEXT,
                manifest_path TEXT,
                official INTEGER NOT NULL DEFAULT 1,
                created_at TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS board_aliases (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                target_family TEXT NOT NULL,
                board_name TEXT NOT NULL,
                alias TEXT NOT NULL,
                trusted INTEGER NOT NULL DEFAULT 0,
                UNIQUE(target_family, alias)
            );

            CREATE TABLE IF NOT EXISTS moderation_log (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                submission_id TEXT REFERENCES submissions(id) ON DELETE CASCADE,
                action TEXT NOT NULL,
                note TEXT,
                created_at TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS bug_reports (
                id TEXT PRIMARY KEY,
                created_at TEXT NOT NULL,
                contact TEXT NOT NULL,
                message TEXT NOT NULL,
                page TEXT NOT NULL,
                user_agent TEXT NOT NULL,
                status TEXT NOT NULL DEFAULT 'new'
            );

            CREATE INDEX IF NOT EXISTS idx_submissions_public_rank
                ON submissions(public, publication_status, scoring_version, mode, submitted_at DESC);
            CREATE INDEX IF NOT EXISTS idx_raw_results_target_family
                ON raw_results(target_family);
            CREATE INDEX IF NOT EXISTS idx_bug_reports_created_at
                ON bug_reports(created_at DESC);
            """
        )
        seed_scoring_metadata(connection)


def seed_scoring_metadata(connection):
    now = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    connection.execute(
        """
        INSERT OR IGNORE INTO reference_sets (id, metrics_json, created_at)
        VALUES (?, ?, ?)
        """,
        (
            REFERENCE_SET_ID,
            json.dumps({
                test_id: {
                    "reference": spec["reference"],
                    "direction": spec["direction"],
                    "category": spec["category"],
                    "weight": spec["weight"],
                }
                for test_id, spec in SCORING_METRICS.items()
            }, separators=(",", ":"), ensure_ascii=False),
            now,
        ),
    )
    connection.execute(
        """
        INSERT OR IGNORE INTO scoring_versions (id, reference_set_id, config_json, active, created_at)
        VALUES (?, ?, ?, 1, ?)
        """,
        (
            SCORING_VERSION,
            REFERENCE_SET_ID,
            json.dumps({
                "submission_schema_version": SUBMISSION_SCHEMA_VERSION,
                "benchmark_profile": BENCHMARK_PROFILE,
                "test_set_id": TEST_SET_ID,
                "category_weights": CATEGORY_WEIGHTS,
                "metric_weights": {
                    test_id: spec["weight"] for test_id, spec in SCORING_METRICS.items()
                },
                "stability": {
                    "free_ratio": 1.10,
                    "max_penalty": 0.05,
                },
            }, separators=(",", ":"), ensure_ascii=False),
            now,
        ),
    )


def public_scoring_config():
    return {
        "submission_schema_version": SUBMISSION_SCHEMA_VERSION,
        "scoring_version": SCORING_VERSION,
        "reference_set_id": REFERENCE_SET_ID,
        "benchmark_profile": BENCHMARK_PROFILE,
        "test_set_id": TEST_SET_ID,
        "mode": SCORING_CONFIG.get("mode", "full"),
        "canonical_value": SCORING_CONFIG.get("canonical_value", "median"),
        "ratio_clamp": SCORING_CONFIG.get("ratio_clamp", {"minimum": 0.25, "maximum": 4.0}),
        "stability": SCORING_CONFIG.get("stability", {}),
        "category_weights": CATEGORY_WEIGHTS,
        "metrics": SCORING_METRICS,
    }


def load_results(limit=500):
    with db_connect() as connection:
        rows = connection.execute(
            """
            SELECT
                s.*,
                rr.raw_json,
                cs.scores_json,
                cs.metric_scores_json,
                cs.normalization_json
            FROM submissions s
            JOIN raw_results rr ON rr.submission_id = s.id
            JOIN computed_scores cs ON cs.submission_id = s.id
            WHERE s.public = 1 AND s.publication_status = 'published'
            ORDER BY s.submitted_at DESC
            LIMIT ?
            """,
            (limit,),
        ).fetchall()
    return [record_from_row(row) for row in rows]


def insert_submission(record):
    with db_connect() as connection:
        connection.execute("BEGIN")
        connection.execute(
            """
            INSERT INTO submissions (
                id, submitted_at, contributor, board_selected_by_user, board_label_trusted,
                mode, benchmark_profile, test_set_id, scoring_version, reference_set_id,
                publication_status, public, transport_json, validation_json
            )
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                record["id"],
                record["submitted_at"],
                record["contributor"],
                record["board_selected_by_user"],
                1 if record["board_label_trusted"] else 0,
                record["mode"],
                record["benchmark_profile"],
                record["test_set_id"],
                record["scoring_version"],
                record["reference_set_id"],
                record["publication"]["status"],
                1 if record["publication"]["public"] else 0,
                json_compact(record["transport"]),
                json_compact(record["validation"]),
            ),
        )
        raw_result = record["raw_result"]
        connection.execute(
            """
            INSERT INTO raw_results (
                submission_id, schema_version, firmware_version, target_family, raw_json
            )
            VALUES (?, ?, ?, ?, ?)
            """,
            (
                record["id"],
                raw_result.get("schema_version"),
                raw_result.get("firmware_version"),
                normalize_soc(raw_result.get("board", {}).get("soc") or raw_result.get("board", {}).get("module")),
                json_compact(raw_result),
            ),
        )
        connection.execute(
            """
            INSERT INTO computed_scores (
                submission_id, scoring_version, reference_set_id, scores_json,
                metric_scores_json, normalization_json, computed_at
            )
            VALUES (?, ?, ?, ?, ?, ?, ?)
            """,
            (
                record["id"],
                record["scoring_version"],
                record["reference_set_id"],
                json_compact(record["scores"]),
                json_compact(record["metric_scores"]),
                json_compact(record["normalization"]),
                record["submitted_at"],
            ),
        )


def validate_bug_report(payload):
    if str(payload.get("bot_field", "")).strip():
        raise ValueError("report rejected")

    validate_challenge(
        str(payload.get("challenge_token", "")),
        str(payload.get("challenge_answer", "")),
    )

    message = str(payload.get("message", "")).strip()
    if not (10 <= len(message) <= 3000):
        raise ValueError("message must be 10 to 3000 characters")

    now = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    return {
        "id": uuid.uuid4().hex,
        "created_at": now,
        "contact": str(payload.get("contact", "")).strip()[:120],
        "message": message,
        "page": str(payload.get("page", "")).strip()[:200],
        "user_agent": str(payload.get("user_agent", "")).strip()[:500],
        "status": "new",
    }


def insert_bug_report(record):
    with db_connect() as connection:
        connection.execute(
            """
            INSERT INTO bug_reports (
                id, created_at, contact, message, page, user_agent, status
            )
            VALUES (?, ?, ?, ?, ?, ?, ?)
            """,
            (
                record["id"],
                record["created_at"],
                record["contact"],
                record["message"],
                record["page"],
                record["user_agent"],
                record["status"],
            ),
        )


def record_from_row(row):
    raw_result = json.loads(row["raw_json"])
    scores = json.loads(row["scores_json"])
    metric_scores = json.loads(row["metric_scores_json"])
    normalization = json.loads(row["normalization_json"])
    validation = json.loads(row["validation_json"])
    transport = json.loads(row["transport_json"])
    return {
        "id": row["id"],
        "submission_schema_version": SUBMISSION_SCHEMA_VERSION,
        "submitted_at": row["submitted_at"],
        "received_at_utc": row["submitted_at"],
        "contributor": row["contributor"],
        "board_selected_by_user": row["board_selected_by_user"],
        "board_label_trusted": bool(row["board_label_trusted"]),
        "mode": row["mode"],
        "benchmark_profile": row["benchmark_profile"],
        "test_set_id": row["test_set_id"],
        "scoring_version": row["scoring_version"],
        "reference_set_id": row["reference_set_id"],
        "publication": {
            "status": row["publication_status"],
            "public": bool(row["public"]),
        },
        "transport": transport,
        "raw_result": raw_result,
        "result": raw_result,
        "scores": scores,
        "metric_scores": metric_scores,
        "normalization": normalization,
        "validation": validation,
    }


def json_compact(value):
    return json.dumps(value, separators=(",", ":"), ensure_ascii=False)


def required_metric_ids():
    return {
        test_id for test_id, spec in SCORING_METRICS.items()
        if spec.get("required")
    }


def validate_result_compatibility(raw_result, metrics):
    issues = []
    if raw_result.get("schema_version") != SUBMISSION_SCHEMA_VERSION:
        issues.append("schema_version mismatch")
    if raw_result.get("benchmark_profile") != BENCHMARK_PROFILE:
        issues.append("benchmark_profile mismatch")
    if raw_result.get("test_set_id") != TEST_SET_ID:
        issues.append("test_set_id mismatch")
    if raw_result.get("mode") != SCORING_CONFIG.get("mode", "full"):
        issues.append("mode mismatch")
    if not raw_result.get("official_generic_firmware"):
        issues.append("official firmware flag missing")

    seen = set()
    duplicates = set()
    numeric_issues = []
    metadata_issues = []
    for metric in metrics:
        if not isinstance(metric, dict):
            numeric_issues.append("invalid metric object")
            continue
        test_id = str(metric.get("test_id", ""))
        if test_id in seen:
            duplicates.add(test_id)
        seen.add(test_id)
        if test_id not in SCORING_METRICS:
            continue
        raw_value = metric_canonical_value(metric)
        if raw_value is None:
            numeric_issues.append(f"{test_id}: missing positive median")
        for key in ("mean", "median", "stdev", "p95", "min", "max"):
            try:
                value = float(metric.get(key))
            except (TypeError, ValueError):
                numeric_issues.append(f"{test_id}: invalid {key}")
                continue
            if not math.isfinite(value) or value < 0:
                numeric_issues.append(f"{test_id}: invalid {key}")
        if metric.get("unit") != "us":
            numeric_issues.append(f"{test_id}: invalid unit")
        if test_id in required_metric_ids():
            try:
                work_units = int(metric.get("work_units", 0))
            except (TypeError, ValueError):
                work_units = 0
            if work_units <= 0 or not str(metric.get("checksum", "")).strip():
                metadata_issues.append(f"{test_id}: missing workload metadata")

    if duplicates:
        issues.append("duplicate metrics: " + ", ".join(sorted(duplicates)))
    if numeric_issues:
        issues.append("invalid metric values")
    if metadata_issues:
        issues.append("missing workload metadata")
    return {
        "issues": issues,
        "numeric_issues": numeric_issues[:20],
        "metadata_issues": metadata_issues[:20],
    }


def validate_submission(payload):
    contributor = str(payload.get("contributor", "")).strip()
    if not (2 <= len(contributor) <= 80):
        raise ValueError("invalid contributor name")

    if str(payload.get("bot_field", "")).strip():
        raise ValueError("bot check failed")

    if int(payload.get("form_elapsed_ms", 0)) < 3000:
        raise ValueError("submission was too fast")

    validate_challenge(
        str(payload.get("challenge_token", "")),
        str(payload.get("challenge_answer", "")),
    )

    raw_result = payload.get("result")
    if not isinstance(raw_result, dict):
        raise ValueError("missing result")

    board = raw_result.get("board")
    config = raw_result.get("config")
    metrics = raw_result.get("results")
    if not isinstance(board, dict) or not isinstance(config, dict) or not isinstance(metrics, list):
        raise ValueError("invalid result structure")
    validate_board_selection(board)

    record = build_submission_record(
        raw_result,
        contributor=contributor,
        transport={
            "kind": "webserial",
            "browser_name": str(payload.get("browser_name", ""))[:80],
            "browser_version": str(payload.get("browser_version", ""))[:80],
            "os": str(payload.get("os", ""))[:80],
        },
        validate_board=True,
        allow_incomplete=False,
        publication_status="published",
        public=True,
    )
    if not record["validation"]["valid_for_leaderboard"]:
        issues = record["validation"].get("issues") or ["result is not valid for leaderboard"]
        raise ValueError("; ".join(issues[:3]))
    return record


def build_submission_record(
    raw_result,
    contributor,
    transport,
    validate_board,
    allow_incomplete,
    publication_status,
    public,
):
    board = raw_result.get("board")
    config = raw_result.get("config")
    metrics = raw_result.get("results")
    if not isinstance(board, dict) or not isinstance(config, dict) or not isinstance(metrics, list):
        raise ValueError("invalid result structure")
    if validate_board:
        validate_board_selection(board)

    required_tests = required_metric_ids()
    submitted_tests = {item.get("test_id") for item in metrics if isinstance(item, dict)}
    missing_tests = sorted(required_tests - submitted_tests)
    if missing_tests and not allow_incomplete:
        raise ValueError("missing benchmark metrics")

    compatibility = validate_result_compatibility(raw_result, metrics)
    issues = list(compatibility["issues"])
    if missing_tests:
        issues.append("missing benchmark metrics")

    computed = compute_scores(raw_result, missing_tests=missing_tests)
    valid_for_leaderboard = not issues and computed["scores"]["espmark_core_score"] is not None
    validation = {
        "required_fields_ok": not compatibility["numeric_issues"],
        "required_metrics_ok": not missing_tests,
        "missing_metrics": missing_tests,
        "issues": issues,
        "numeric_issues": compatibility["numeric_issues"],
        "metadata_issues": compatibility["metadata_issues"],
        "official_firmware_ok": bool(raw_result.get("official_generic_firmware", False)),
        "compatible_profile_ok": raw_result.get("benchmark_profile") == BENCHMARK_PROFILE
            and raw_result.get("test_set_id") == TEST_SET_ID
            and raw_result.get("mode") == SCORING_CONFIG.get("mode", "full"),
        "known_answers_ok": not compatibility["metadata_issues"],
        "score_computed_by_backend": True,
        "valid_for_leaderboard": valid_for_leaderboard,
    }

    submitted_at = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    record_id = uuid.uuid4().hex
    return {
        "id": record_id,
        "submission_schema_version": SUBMISSION_SCHEMA_VERSION,
        "submitted_at": submitted_at,
        "received_at_utc": submitted_at,
        "contributor": contributor,
        "board_selected_by_user": board.get("name", ""),
        "board_label_trusted": False,
        "mode": raw_result.get("mode", "full"),
        "benchmark_profile": raw_result.get("benchmark_profile", BENCHMARK_PROFILE),
        "test_set_id": raw_result.get("test_set_id", TEST_SET_ID),
        "scoring_version": SCORING_VERSION,
        "reference_set_id": REFERENCE_SET_ID,
        "publication": {
            "status": publication_status,
            "public": public,
        },
        "transport": transport,
        "raw_result": raw_result,
        "result": raw_result,
        "scores": computed["scores"],
        "metric_scores": computed["metric_scores"],
        "normalization": computed["normalization"],
        "validation": validation,
    }


def normalize_stored_record(record):
    if "raw_result" in record and "scores" in record:
        record.setdefault("result", record.get("raw_result"))
        record.setdefault("submitted_at", record.get("received_at_utc", ""))
        return record

    raw_result = record.get("result")
    if not isinstance(raw_result, dict):
        return record

    required_tests = {
        test_id for test_id, spec in SCORING_METRICS.items()
        if spec.get("required")
    }
    submitted_tests = {
        item.get("test_id") for item in raw_result.get("results", [])
        if isinstance(item, dict)
    }
    missing_tests = sorted(required_tests - submitted_tests)
    computed = compute_scores(raw_result, missing_tests=missing_tests)
    record = dict(record)
    record.setdefault("submission_schema_version", "legacy")
    record.setdefault("received_at_utc", record.get("submitted_at", ""))
    record.setdefault("mode", raw_result.get("mode", "full"))
    record.setdefault("benchmark_profile", raw_result.get("benchmark_profile", BENCHMARK_PROFILE))
    record.setdefault("test_set_id", raw_result.get("test_set_id", TEST_SET_ID))
    record.setdefault("scoring_version", SCORING_VERSION)
    record.setdefault("reference_set_id", REFERENCE_SET_ID)
    record.setdefault("board_selected_by_user", raw_result.get("board", {}).get("name", ""))
    record.setdefault("board_label_trusted", False)
    record.setdefault("raw_result", raw_result)
    record.setdefault("scores", computed["scores"])
    record.setdefault("metric_scores", computed["metric_scores"])
    record.setdefault("normalization", computed["normalization"])
    record.setdefault("validation", {
        "required_fields_ok": True,
        "required_metrics_ok": True,
        "missing_metrics": missing_tests,
        "official_firmware_ok": bool(raw_result.get("official_generic_firmware", False)),
        "known_answers_ok": True,
        "score_computed_by_backend": True,
        "valid_for_leaderboard": not missing_tests and computed["scores"]["espmark_core_score"] is not None,
    })
    return record


def compute_scores(raw_result, missing_tests=None):
    missing_tests = missing_tests or []
    metrics = raw_result.get("results", [])
    metric_scores = {}
    category_inputs = {}
    stability_inputs = []

    for metric in metrics:
        if not isinstance(metric, dict):
            continue
        test_id = str(metric.get("test_id", ""))
        spec = SCORING_METRICS.get(test_id)
        if not spec:
            continue

        raw_value = metric_canonical_value(metric)
        reference = float(spec["reference"])
        if raw_value is None or reference <= 0:
            continue

        if spec["direction"] == "higher_is_better":
            ratio = raw_value / reference
        else:
            ratio = reference / raw_value
        ratio_clamp = SCORING_CONFIG.get("ratio_clamp", {})
        ratio = clamp(
            ratio,
            float(ratio_clamp.get("minimum", 0.25)),
            float(ratio_clamp.get("maximum", 4.0)),
        )
        score = ratio * 1000.0
        category = spec["category"]
        weight = float(spec["weight"])
        category_inputs.setdefault(category, []).append((ratio, weight))

        stability_ratio = metric_stability_ratio(metric)
        if stability_ratio:
            stability_inputs.append((stability_ratio, weight))

        metric_scores[test_id] = {
            "label": spec["label"],
            "category": category,
            "raw_value": round(raw_value, 3),
            "reference": reference,
            "direction": spec["direction"],
            "normalized_ratio": round(ratio, 6),
            "score": round(score, 1),
            "weight": weight,
        }

    category_scores = {}
    core_inputs = []
    for category, inputs in category_inputs.items():
        category_ratio = weighted_geomean(inputs)
        category_score = category_ratio * 1000.0
        category_scores[f"{category}_score"] = round(category_score, 1)
        core_weight = CATEGORY_WEIGHTS.get(category)
        if core_weight:
            core_inputs.append((category_score / 1000.0, float(core_weight)))

    stability_factor = compute_stability_factor(stability_inputs)
    core_score = None
    if core_inputs and not missing_tests:
        core_score = weighted_geomean(core_inputs) * 1000.0 * stability_factor

    scores = {
        "espmark_core_score": round(core_score, 1) if core_score is not None else None,
        "cpu_score": category_scores.get("cpu_score"),
        "memory_score": category_scores.get("memory_score"),
        "flash_score": category_scores.get("flash_score"),
        "practical_iot_score": category_scores.get("practical_iot_score"),
        "optional_environment_score": None,
        "stability_factor": round(stability_factor, 4),
    }

    target_family = normalize_soc(raw_result.get("board", {}).get("soc") or raw_result.get("board", {}).get("module"))
    mode = raw_result.get("mode", "full")
    return {
        "scores": scores,
        "metric_scores": metric_scores,
        "normalization": {
            "reference_set_id": REFERENCE_SET_ID,
            "scoring_version": SCORING_VERSION,
            "benchmark_profile": raw_result.get("benchmark_profile", BENCHMARK_PROFILE),
            "test_set_id": raw_result.get("test_set_id", TEST_SET_ID),
            "leaderboard_key": f"{SCORING_VERSION}|{mode}",
            "family_leaderboard_key": f"{target_family}|{SCORING_VERSION}|{mode}",
        },
    }


def metric_canonical_value(metric):
    for key in ("median", "mean"):
        try:
            value = float(metric.get(key))
        except (TypeError, ValueError):
            continue
        if value > 0:
            return value
    return None


def metric_stability_ratio(metric):
    try:
        median = float(metric.get("median"))
        p95 = float(metric.get("p95"))
    except (TypeError, ValueError):
        return None
    if median <= 0 or p95 <= 0:
        return None
    return max(1.0, p95 / median)


def compute_stability_factor(inputs):
    if not inputs:
        return 1.0
    stability = SCORING_CONFIG.get("stability", {})
    free_ratio = float(stability.get("free_ratio", 1.10))
    penalty_per_ratio = float(stability.get("penalty_per_ratio", 0.25))
    max_penalty = float(stability.get("max_penalty", 0.05))
    stability_ratio = weighted_geomean(inputs)
    if stability_ratio <= free_ratio:
        return 1.0
    return max(1.0 - max_penalty, 1.0 - penalty_per_ratio * (stability_ratio - free_ratio))


def weighted_geomean(inputs):
    total_weight = sum(weight for _, weight in inputs if weight > 0)
    if total_weight <= 0:
        return 1.0
    log_sum = sum(weight * math.log(max(float(value), 0.000001)) for value, weight in inputs if value > 0 and weight > 0)
    return math.exp(log_sum / total_weight)


def clamp(value, minimum, maximum):
    return max(minimum, min(maximum, value))


def load_board_catalog():
    try:
        return json.loads(BOARDS_FILE.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {"families": {}}


def normalize_soc(value):
    soc = str(value or "").strip().upper()
    if soc.startswith("ESP32") and len(soc) > 5 and soc[5] != "-":
        soc = f"ESP32-{soc[5:]}"
    if soc == "ESP32" or soc.startswith("ESP32-D") or soc.startswith("ESP32-U") or soc.startswith("ESP32-PICO"):
        return "ESP32"
    return soc


def validate_board_selection(board):
    board_id = str(board.get("id", "")).strip()
    board_name = str(board.get("name", "")).strip()
    soc = normalize_soc(board.get("soc") or board.get("module"))
    if not board_id or not board_name:
        raise ValueError("exact board selection is required")

    if board_id.startswith("custom:") and str(board.get("board_custom_label", "")).strip():
        if not (2 <= len(board_name) <= 120):
            raise ValueError("custom board name is invalid")
        return

    family_boards = load_board_catalog().get("families", {}).get(soc, [])
    matching_board = next((item for item in family_boards if item.get("id") == board_id), None)
    if not matching_board:
        raise ValueError("selected board is not valid for this chip")
    if matching_board.get("name") != board_name:
        raise ValueError("selected board name does not match catalog")


def create_challenge():
    left = secrets.randbelow(9) + 2
    right = secrets.randbelow(9) + 2
    op = secrets.choice(["+", "-"])
    if op == "-" and right > left:
        left, right = right, left

    answer = left + right if op == "+" else left - right
    nonce = secrets.token_hex(12)
    payload = {
        "answer_hash": hash_challenge_answer(answer, nonce),
        "expires": int(time.time()) + CHALLENGE_TTL_SECONDS,
        "nonce": nonce,
    }
    return {
        "question": f"{left} {op} {right} =",
        "token": sign_payload(payload),
    }


def sign_payload(payload):
    body = json.dumps(payload, separators=(",", ":"), sort_keys=True).encode("utf-8")
    body_token = base64.urlsafe_b64encode(body).decode("ascii").rstrip("=")
    signature = hmac.new(CHALLENGE_SECRET.encode("utf-8"), body_token.encode("ascii"), hashlib.sha256)
    sig_token = base64.urlsafe_b64encode(signature.digest()).decode("ascii").rstrip("=")
    return f"{body_token}.{sig_token}"


def hash_challenge_answer(answer, nonce):
    message = f"{answer}.{nonce}".encode("utf-8")
    return hmac.new(CHALLENGE_SECRET.encode("utf-8"), message, hashlib.sha256).hexdigest()


def validate_challenge(token, answer):
    cleanup_used_challenges()
    try:
        body_token, sig_token = token.split(".", 1)
    except ValueError:
        raise ValueError("bot check failed")

    expected = hmac.new(CHALLENGE_SECRET.encode("utf-8"), body_token.encode("ascii"), hashlib.sha256)
    expected_sig = base64.urlsafe_b64encode(expected.digest()).decode("ascii").rstrip("=")
    if not hmac.compare_digest(sig_token, expected_sig):
        raise ValueError("bot check failed")

    try:
        padded = body_token + ("=" * (-len(body_token) % 4))
        payload = json.loads(base64.urlsafe_b64decode(padded.encode("ascii")).decode("utf-8"))
    except (ValueError, json.JSONDecodeError, binascii.Error):
        raise ValueError("bot check failed")

    if int(payload.get("expires", 0)) < int(time.time()):
        raise ValueError("bot check expired")

    nonce = str(payload.get("nonce", ""))
    used_key = f"{nonce}:{payload.get('expires', 0)}"
    if not nonce or used_key in USED_CHALLENGE_NONCES:
        raise ValueError("bot check failed")

    try:
        submitted_answer = int(answer.strip())
    except ValueError:
        raise ValueError("bot check failed")

    expected_hash = str(payload.get("answer_hash", ""))
    submitted_hash = hash_challenge_answer(submitted_answer, nonce)
    if not hmac.compare_digest(submitted_hash, expected_hash):
        raise ValueError("bot check failed")

    USED_CHALLENGE_NONCES.add(used_key)


def cleanup_used_challenges():
    now = int(time.time())
    expired = {
        key for key in USED_CHALLENGE_NONCES
        if int(key.rsplit(":", 1)[1]) < now
    }
    USED_CHALLENGE_NONCES.difference_update(expired)


def main():
    init_database()
    server = ThreadingHTTPServer(("0.0.0.0", 8000), EspmarkHandler)
    print("espmark web server listening on :8000", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
