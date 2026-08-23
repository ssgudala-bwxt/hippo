#!/usr/bin/env bash
# Wrapper for cube_slice case directory — mirrors tests file behavior.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd "${SCRIPT_DIR}"

step() { echo; echo "=== $* ==="; }

run_setup() {
  step "setup: blockMesh + decomposePar"
  blockMesh -case foaminput
  decomposePar -case foaminput -force
}

run_case() {
  step "run: hippo-opt (n=4)"
  srun --mpi=pmi2 -K -n 4 hippo-opt -i run.i --allow-test-objects
}

run_verify() {
  step "verify_num_nodes"
  ../scripts/check.sh config run_out.e
}

run_all() {
  run_setup
  run_case
  run_verify
}

MODE="${1:-all}"
case "$MODE" in
  all)
    run_all
    ;;
  setup)
    run_setup
    ;;
  run)
    run_case
    ;;
  verify)
    run_verify
    ;;
  *)
    echo "Usage: $0 [all|setup|run|verify]"
    exit 1
    ;;
esac

echo
echo "=== ALL STEPS PASSED ==="