import importlib.util
import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SERVER_PATH = ROOT / "web" / "server.py"

spec = importlib.util.spec_from_file_location("espmark_server", SERVER_PATH)
server = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)


def reference_result(scale=1.0, overrides=None):
    metrics = []
    for test_id, metric_spec in server.SCORING_METRICS.items():
        if not metric_spec.get("required"):
            continue
        median = float(metric_spec["reference"]) * scale
        metrics.append({
            "test_id": test_id,
            "category": metric_spec["category"],
            "unit": "us",
            "samples": 12,
            "mean": median,
            "median": median,
            "stdev": 0.0,
            "p95": median,
            "min": median,
            "max": median,
            "work_units": 100,
            "checksum": "0x12345678",
        })
    result = {
        "schema_version": server.SUBMISSION_SCHEMA_VERSION,
        "firmware_version": "0.2.2-arduino",
        "benchmark_profile": server.BENCHMARK_PROFILE,
        "test_set_id": server.TEST_SET_ID,
        "mode": "full",
        "official_generic_firmware": True,
        "board": {
            "vendor": "Generic",
            "name": "Generic ESP32",
            "module": "ESP32",
            "soc": "ESP32",
            "revision": 1,
        },
        "config": {
            "cpu_freq_mhz": 240,
            "memory_block_bytes": 16384,
        },
        "results": metrics,
    }
    if overrides:
        result.update(overrides)
    return result


class ScoringTests(unittest.TestCase):
    def test_reference_scores_to_1000(self):
        computed = server.compute_scores(reference_result())
        self.assertEqual(computed["scores"]["espmark_core_score"], 1000.0)
        self.assertEqual(computed["scores"]["stability_factor"], 1.0)

    def test_half_time_scores_to_2000(self):
        computed = server.compute_scores(reference_result(scale=0.5))
        self.assertEqual(computed["scores"]["espmark_core_score"], 2000.0)

    def test_double_time_scores_to_500(self):
        computed = server.compute_scores(reference_result(scale=2.0))
        self.assertEqual(computed["scores"]["espmark_core_score"], 500.0)

    def test_ratio_clamp(self):
        fast = server.compute_scores(reference_result(scale=0.01))
        slow = server.compute_scores(reference_result(scale=100.0))
        self.assertEqual(fast["scores"]["espmark_core_score"], 4000.0)
        self.assertEqual(slow["scores"]["espmark_core_score"], 250.0)

    def test_missing_required_metric_blocks_core_score(self):
        result = reference_result()
        missing = result["results"].pop()["test_id"]
        computed = server.compute_scores(result, missing_tests=[missing])
        self.assertIsNone(computed["scores"]["espmark_core_score"])

    def test_incompatible_profile_is_not_valid_for_leaderboard(self):
        result = reference_result(overrides={"test_set_id": "wrong"})
        record = server.build_submission_record(
            result,
            contributor="Preview",
            transport={"kind": "test"},
            validate_board=False,
            allow_incomplete=True,
            publication_status="preview",
            public=False,
        )
        self.assertFalse(record["validation"]["valid_for_leaderboard"])
        self.assertIn("test_set_id mismatch", record["validation"]["issues"])

    def test_missing_workload_metadata_is_not_valid_for_leaderboard(self):
        result = reference_result()
        result["results"][0].pop("checksum")
        record = server.build_submission_record(
            result,
            contributor="Preview",
            transport={"kind": "test"},
            validate_board=False,
            allow_incomplete=True,
            publication_status="preview",
            public=False,
        )
        self.assertFalse(record["validation"]["valid_for_leaderboard"])
        self.assertFalse(record["validation"]["known_answers_ok"])


if __name__ == "__main__":
    unittest.main()
