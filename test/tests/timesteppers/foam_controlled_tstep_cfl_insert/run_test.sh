#!/usr/bin/env bash
# Wrapper for test/tests/timesteppers/foam_controlled_tstep_cfl_insert —
# replicates the TestHarness pipeline (tests file:
# foam_controlled_tstep_cfl_insert/{setup,run,verify,
# setup_no_adaptive,run_no_adaptive,verify_no_adaptive,
# setup_foam_only,run_foam_only,verify_foam_only}).
# Usage: bash run_test.sh [all|setup|run|verify|
#                          setup_no_adaptive|run_no_adaptive|verify_no_adaptive|
#                          setup_foam_only|run_foam_only|verify_foam_only]

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HIPPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
PYTHONPATH="${HIPPO_ROOT}/python:${PYTHONPATH:-}"
export PYTHONPATH

cd "${SCRIPT_DIR}"

step() { echo; echo "=== $* ==="; }

PASS=0
FAIL=0

clean_and_mesh() {
  (cd fluid-openfoam && ./Allclean)
  blockMesh -case fluid-openfoam
}

run_setup() {
  step "setup: fluid-openfoam Allclean + blockMesh"
  clean_and_mesh
}

run_run() {
  step "run: hippo-opt (serial) run.i"
  if hippo-opt -i run.i; then
    echo "PASS (run)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (run): hippo-opt exited non-zero"
    FAIL=$((FAIL + 1))
  fi
}

run_verify() {
  step "verify: test_synchronisation_and_cutback"
  if python3 -m pytest test.py -v -k test_synchronisation_and_cutback; then
    echo "PASS (verify)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (verify)"
    FAIL=$((FAIL + 1))
  fi
}

run_setup_no_adaptive() {
  step "setup_no_adaptive: fluid-openfoam Allclean + blockMesh"
  clean_and_mesh
}

run_run_no_adaptive() {
  step "run_no_adaptive: hippo-opt (serial) run.i with Executioner/dt=0.03"
  if hippo-opt -i run.i "MultiApps/hippo/cli_args=Executioner/dt=0.03"; then
    echo "PASS (run_no_adaptive)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (run_no_adaptive): hippo-opt exited non-zero"
    FAIL=$((FAIL + 1))
  fi
}

run_verify_no_adaptive() {
  step "verify_no_adaptive: test_synchronisation_and_cutback + test_force_no_cfl"
  if python3 -m pytest test.py -v -k "test_synchronisation_and_cutback or test_force_no_cfl"; then
    echo "PASS (verify_no_adaptive)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (verify_no_adaptive)"
    FAIL=$((FAIL + 1))
  fi
}

run_setup_foam_only() {
  step "setup_foam_only: fluid-openfoam Allclean + blockMesh"
  clean_and_mesh
}

run_run_foam_only() {
  step "run_foam_only: hippo-opt (serial) fluid.i"
  if hippo-opt -i fluid.i; then
    echo "PASS (run_foam_only)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (run_foam_only): hippo-opt exited non-zero"
    FAIL=$((FAIL + 1))
  fi
}

run_verify_foam_only() {
  step "verify_foam_only: test_foam_only"
  if python3 -m pytest test.py -v -k test_foam_only; then
    echo "PASS (verify_foam_only)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (verify_foam_only)"
    FAIL=$((FAIL + 1))
  fi
}

summary() {
  echo
  echo "=== Summary: ${PASS} passed, ${FAIL} failed ==="
  [ "${FAIL}" -eq 0 ]
}

run_all() {
  run_setup
  run_run
  run_verify
  run_setup_no_adaptive
  run_run_no_adaptive
  run_verify_no_adaptive
  run_setup_foam_only
  run_run_foam_only
  run_verify_foam_only
  summary
}

MODE="${1:-all}"
case "$MODE" in
  all) run_all ;;
  setup) run_setup ;;
  run) run_run ;;
  verify) run_verify ;;
  setup_no_adaptive) run_setup_no_adaptive ;;
  run_no_adaptive) run_run_no_adaptive ;;
  verify_no_adaptive) run_verify_no_adaptive ;;
  setup_foam_only) run_setup_foam_only ;;
  run_foam_only) run_run_foam_only ;;
  verify_foam_only) run_verify_foam_only ;;
  *)
    echo "Usage: $0 [all|setup|run|verify|setup_no_adaptive|run_no_adaptive|verify_no_adaptive|setup_foam_only|run_foam_only|verify_foam_only]"
    exit 1
    ;;
esac
RESULT=$?

echo
if [ "${RESULT}" -eq 0 ]; then
  echo "=== ALL STEPS PASSED ==="
else
  echo "=== FAILURES DETECTED ==="
  exit 1
fi
