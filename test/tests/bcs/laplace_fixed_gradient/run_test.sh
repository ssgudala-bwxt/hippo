#!/usr/bin/env bash
# Wrapper for test/tests/bcs/laplace_fixed_gradient — replicates the TestHarness pipeline.
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

run_verify() {
  step "verify: analytical comparison"
  python3 -m pytest test.py -v
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
