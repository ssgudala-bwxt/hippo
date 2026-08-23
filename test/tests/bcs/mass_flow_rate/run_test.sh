#!/usr/bin/env bash
# Wrapper for test/tests/bcs/mass_flow_rate — replicates the TestHarness pipeline.
# Usage: bash run_test.sh [all|setup|run|verify]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HIPPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
PYTHONPATH="${HIPPO_ROOT}/python:${PYTHONPATH:-}"
export PYTHONPATH

cd "${SCRIPT_DIR}"

step() { echo; echo "=== $* ==="; }

run_setup() {
  step "setup: foamCleanCase + blockMesh"
  foamCleanCase -case foam
  blockMesh -case foam
}

run_run() {
  step "run: hippo-opt (serial)"
  srun --mpi=pmi2 -K -n 1 hippo-opt -i main.i
}

# Mirrors MOOSE's CSVDiff tester (default rel_err=5.5e-6, abs_zero=1e-10).
run_verify() {
  step "verify: csvdiff main_out.csv against gold/main_out.csv"
  python3 - "gold/main_out.csv" "main_out.csv" <<'PYEOF'
import csv
import sys

gold_path, out_path = sys.argv[1], sys.argv[2]
rel_err = 5.5e-6
abs_zero = 1e-10

with open(gold_path, newline="", encoding="utf-8") as f:
    gold_rows = list(csv.DictReader(f))
with open(out_path, newline="", encoding="utf-8") as f:
    out_rows = list(csv.DictReader(f))

if len(gold_rows) != len(out_rows):
    sys.exit(
        f"FAILED: row count mismatch: gold={len(gold_rows)} out={len(out_rows)}"
    )

failures = []
for i, (gold_row, out_row) in enumerate(zip(gold_rows, out_rows)):
    for key in gold_row:
        gold_val = float(gold_row[key])
        out_val = float(out_row[key])
        if abs(gold_val) < abs_zero and abs(out_val) < abs_zero:
            continue
        diff = abs(gold_val - out_val)
        tol = rel_err * max(abs(gold_val), abs(out_val))
        if diff > tol:
            failures.append(
                f"row {i} column '{key}': gold={gold_val} out={out_val} diff={diff} tol={tol}"
            )

if failures:
    print("FAILED:")
    for failure in failures:
        print(f"  {failure}")
    sys.exit(1)

print("PASS: main_out.csv matches gold/main_out.csv")
PYEOF
}

run_all() {
  run_setup
  run_run
  run_verify
}

MODE="${1:-all}"
case "$MODE" in
  all) run_all ;;
  setup) run_setup ;;
  run) run_run ;;
  verify) run_verify ;;
  *)
    echo "Usage: $0 [all|setup|run|verify]"
    exit 1
    ;;
esac

echo
echo "=== ALL STEPS PASSED ==="
