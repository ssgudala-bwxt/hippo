#!/usr/bin/env bash
# Wrapper for test/tests/kernels/simple_diffusion — replicates the TestHarness
# pipeline (tests file: 'run' RunApp of simple_diffusion.i, then 'test'
# PythonUnitTest of test.py). This is a pure MOOSE test with no OpenFOAM case
# directory — no mesh/decomposePar setup is needed.
# Usage: bash run_test.sh [run|verify|all]
#   run     runs hippo-opt (serial, n=1)
#   verify  runs the analytical-solution pytest check
#   all     run + verify

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HIPPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
PYTHONPATH="${HIPPO_ROOT}/python:${PYTHONPATH:-}"
export PYTHONPATH

MODE="${1:-all}"

cd "${SCRIPT_DIR}"

step() { echo; echo "=== $* ==="; }

clean_case() {
  rm -f simple_diffusion_out.e
  rm -rf Outputs
}

run_app() {
  step "run: hippo-opt (serial)"
  clean_case
  srun --mpi=pmi2 -K -n 1 hippo-opt -i simple_diffusion.i
}

run_verify() {
  step "verify: analytical comparison"
  python3 -m pytest test.py -v
}

case "$MODE" in
  run)
    run_app
    ;;
  verify)
    run_verify
    ;;
  all)
    run_app
    run_verify
    ;;
  *)
    echo "Usage: $0 [run|verify|all]"
    exit 1
    ;;
esac

echo
echo "=== ALL STEPS PASSED ==="
