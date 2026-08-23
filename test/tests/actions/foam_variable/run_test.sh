#!/usr/bin/env bash
# Wrapper for test/tests/actions/foam_variable — mirrors tests file behavior.
# Usage: bash run_test.sh [all|setup|run|err_problem]

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
  step "setup: foamCleanCase + blockMesh"
  foamCleanCase -case foam
  blockMesh -case foam
}

run_run() {
  step "run: hippo-opt (serial)"
  srun --mpi=pmi2 -K -n 1 hippo-opt -i main.i --allow-test-objects
}

# run_expect_err <name> <input file> <expected substring> [extra cli_args...]
run_expect_err() {
  local name="$1"; shift
  local input="$1"; shift
  local expect="$1"; shift
  step "${name}: expecting error containing: ${expect}"
  local output
  if output=$(srun --mpi=pmi2 -K -n 1 hippo-opt -i "${input}" --allow-test-objects "$@" 2>&1); then
    echo "FAILED (${name}): expected an error but command succeeded"
    echo "${output}"
    FAIL=$((FAIL + 1))
    return
  fi
  if echo "${output}" | grep -qF "${expect}"; then
    echo "PASS (${name})"
    PASS=$((PASS + 1))
  else
    echo "FAILED (${name}): expected error text not found"
    echo "${output}"
    FAIL=$((FAIL + 1))
  fi
}

run_err_problem() {
  run_expect_err "err_problem" main.i "can only be used with FoamProblem" \
    Problem/type=FEProblem
}

summary() {
  echo
  echo "=== Summary: ${PASS} passed, ${FAIL} failed ==="
  [ "${FAIL}" -eq 0 ]
}

run_all() {
  run_setup
  run_run
  run_err_problem
  summary
}

MODE="${1:-all}"
case "$MODE" in
  all) run_all ;;
  setup) run_setup ;;
  run) run_run ;;
  err_problem) run_err_problem; summary ;;
  *)
    echo "Usage: $0 [all|setup|run|err_problem]"
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
