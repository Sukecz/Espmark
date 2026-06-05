#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CATALOG="$ROOT_DIR/web/boards.json"

python3 "$ROOT_DIR/scripts/generate_boards_catalog.py" "$@"
python3 -m json.tool "$CATALOG" >/dev/null

python3 - "$CATALOG" <<'PY'
import json
import sys

catalog_path = sys.argv[1]
with open(catalog_path, encoding="utf-8") as handle:
    catalog = json.load(handle)

print("Updated board catalog:")
for family, boards in catalog["families"].items():
    print(f"- {family}: {len(boards)} boards")
PY
