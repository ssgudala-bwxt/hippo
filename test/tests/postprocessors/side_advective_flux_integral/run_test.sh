#!/usr/bin/env bash
# Wrapper for test/tests/postprocessors/side_advective_flux_integral — replicates
# the TestHarness pipeline (tests file: postprocessor_test/{setup,run,
# verify_serial,run_parallel,verify_parallel,invalid_advective_velocity,
# invalid_scalar}).
# Usage: bash run_test.sh [all|setup|run|verify_serial|run_parallel|verify_parallel|invalid_advective_velocity|invalid_scalar]

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HIPPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
PYTHONPATH="${HIPPO_ROOT}/python:${PYTHONPATH:-}"
export PYTHONPATH

cd "${SCRIPT_DIR}"

step() { echo; echo "=== $* ==="; }

# foamCleanCase is not available in all OpenFOAM installs (e.g. minimal/site
# builds without bin/tools/CleanFunctions). Reimplement the equivalent
# cleanup manually: drop generated time directories (keep the initial "0"),
# decomposed processor dirs, and other run artifacts.
clean_case() {
  local dir="$1"
  rm -rf "${dir}"/constant/polyMesh "${dir}"/processor* "${dir}"/postProcessing \
         "${dir}"/VTK "${dir}"/dynamicCode "${dir}"/probes "${dir}"/*.foam "${dir}"/log.*
  local d
  for d in "${dir}"/[0-9]*; do
    [ -e "$d" ] || continue
    [ "$(basename "$d")" = "0" ] && continue
    rm -rf "$d"
  done
}

PASS=0
FAIL=0

run_setup() {
  step "setup: clean_case + blockMesh + decomposePar"
  clean_case foam
  blockMesh -case foam
  decomposePar -force -case foam
  rm -f main_out.csv
}

run_run() {
  step "run: hippo-opt (serial)"
  if srun --mpi=pmi2 -K -n 1 hippo-opt -i main.i; then
    echo "PASS (run)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (run): hippo-opt exited non-zero"
    FAIL=$((FAIL + 1))
  fi
}

run_verify_serial() {
  step "verify_serial: postprocessor csv comparison"
  if python3 -m pytest test.py -v; then
    echo "PASS (verify_serial)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (verify_serial)"
    FAIL=$((FAIL + 1))
  fi
}

run_run_parallel() {
  step "run_parallel: hippo-opt (n=2)"
  if srun --mpi=pmi2 -K -n 2 hippo-opt -i main.i; then
    echo "PASS (run_parallel)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (run_parallel): hippo-opt exited non-zero"
    FAIL=$((FAIL + 1))
  fi
}

run_verify_parallel() {
  step "verify_parallel: postprocessor csv comparison"
  if python3 -m pytest test.py -v; then
    echo "PASS (verify_parallel)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (verify_parallel)"
    FAIL=$((FAIL + 1))
  fi
}

# run_expect_err <name> <expected substring> [extra cli_args...]
run_expect_err() {
  local name="$1"; shift
  local expect="$1"; shift
  step "${name}: expecting error containing: ${expect}"
  local output
  if output=$(srun --mpi=pmi2 -K -n 1 hippo-opt -i main.i "$@" 2>&1); then
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

run_invalid_advective_velocity() {
  run_expect_err "invalid_advective_velocity" "advective_velocity 'U1' not found." \
    Postprocessors/m_dot_x1/advective_velocity=U1
}

run_invalid_scalar() {
  run_expect_err "invalid_scalar" "foam_scalar 'rho1' not found." \
    Postprocessors/m_dot_x1/foam_scalar=rho1
}

summary() {
  echo
  echo "=== Summary: ${PASS} passed, ${FAIL} failed ==="
  [ "${FAIL}" -eq 0 ]
}

run_all() {
  run_setup
  run_run
  run_verify_serial
  run_setup
  run_run_parallel
  run_verify_parallel
  run_setup
  run_invalid_advective_velocity
  run_invalid_scalar
  summary
}

MODE="${1:-all}"
case "$MODE" in
  all) run_all ;;
  setup) run_setup ;;
  run) run_run ;;
  verify_serial) run_verify_serial ;;
  run_parallel) run_run_parallel ;;
  verify_parallel) run_verify_parallel ;;
  invalid_advective_velocity) run_invalid_advective_velocity; summary ;;
  invalid_scalar) run_invalid_scalar; summary ;;
  *)
    echo "Usage: $0 [all|setup|run|verify_serial|run_parallel|verify_parallel|invalid_advective_velocity|invalid_scalar]"
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
