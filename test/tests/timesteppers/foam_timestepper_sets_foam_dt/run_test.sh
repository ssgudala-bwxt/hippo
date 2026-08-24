#!/usr/bin/env bash
# Wrapper for test/tests/timesteppers/foam_timestepper_sets_foam_dt —
# replicates the TestHarness pipeline (tests file:
# foam_timestepper_sets_foam_t_and_dt/{setup,run,verify}).
# Usage: bash run_test.sh [all|setup|run|verify]

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HIPPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
PYTHONPATH="${HIPPO_ROOT}/python:${PYTHONPATH:-}"
export PYTHONPATH

cd "${SCRIPT_DIR}"

step() { echo; echo "=== $* ==="; }

PASS=0
FAIL=0

run_setup() {
  step "setup: buoyantCavity Allclean + blockMesh"
  (cd buoyantCavity && ./Allclean)
  blockMesh -case buoyantCavity
}

run_run() {
  step "run: hippo-opt (serial)"
  if hippo-opt -i run.i; then
    echo "PASS (run)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (run): hippo-opt exited non-zero"
    FAIL=$((FAIL + 1))
  fi
}

run_verify() {
  step "verify: check time-step directories match Executioner block"
  if bash verify.sh; then
    echo "PASS (verify)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (verify)"
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
  summary
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
RESULT=$?

echo
if [ "${RESULT}" -eq 0 ]; then
  echo "=== ALL STEPS PASSED ==="
else
  echo "=== FAILURES DETECTED ==="
  exit 1
fi
