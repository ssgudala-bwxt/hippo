#!/usr/bin/env bash
# Wrapper for test/tests/actions/foam_bc — mirrors tests file behavior.
# Usage: bash run_test.sh [all|setup|syntax|foam_var_error|foam_boundary_error|
#                          foam_duplicated_boundary_error|order_of_t_is_not_constant|
#                          order_of_t_is_not_monomial|v_doesnt_exist]

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
  step "setup: clean_case + blockMesh"
  clean_case foam
  blockMesh -case foam
}

run_syntax() {
  step "check_foam_bc_input_syntax: hippo-opt (serial)"
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

run_foam_var_error() {
  run_expect_err "foam_var_error" main.i "There is no OpenFOAM field named 'T1'" \
    FoamBCs/temp/foam_variable=T1
}

run_foam_boundary_error() {
  run_expect_err "foam_boundary_error" main.i "Boundary 'left1' not found in FoamMesh" \
    FoamBCs/temp/boundary=left1
}

run_foam_duplicated_boundary_error() {
  run_expect_err "foam_duplicated_boundary_error" main.i \
    "Imposed FoamBC has duplicated boundary 'left' for foam variable 'T'" \
    FoamBCs/temp_r/boundary=left
}

run_order_of_t_is_not_constant() {
  run_expect_err "order_of_t_is_not_constant" order_of_t_is_not_constant.i \
    "must be a constant monomial" AuxVariables/T/order=FIRST
}

run_order_of_t_is_not_monomial() {
  run_expect_err "order_of_t_is_not_monomial" order_of_t_is_not_constant.i \
    "must be a constant monomial" AuxVariables/T/family=LAGRANGE AuxVariables/T/order=FIRST
}

run_v_doesnt_exist() {
  run_expect_err "v_doesnt_exist" order_of_t_is_not_constant.i \
    "Variable 'T1' doesn't exist" FoamBCs/T/v=T1
}

summary() {
  echo
  echo "=== Summary: ${PASS} passed, ${FAIL} failed ==="
  [ "${FAIL}" -eq 0 ]
}

run_all() {
  run_setup
  run_syntax
  run_foam_var_error
  run_foam_boundary_error
  run_foam_duplicated_boundary_error
  run_order_of_t_is_not_constant
  run_order_of_t_is_not_monomial
  run_v_doesnt_exist
  summary
}

MODE="${1:-all}"
case "$MODE" in
  all) run_all ;;
  setup) run_setup ;;
  syntax) run_syntax ;;
  foam_var_error) run_foam_var_error; summary ;;
  foam_boundary_error) run_foam_boundary_error; summary ;;
  foam_duplicated_boundary_error) run_foam_duplicated_boundary_error; summary ;;
  order_of_t_is_not_constant) run_order_of_t_is_not_constant; summary ;;
  order_of_t_is_not_monomial) run_order_of_t_is_not_monomial; summary ;;
  v_doesnt_exist) run_v_doesnt_exist; summary ;;
  *)
    echo "Usage: $0 [all|setup|syntax|foam_var_error|foam_boundary_error|foam_duplicated_boundary_error|order_of_t_is_not_constant|order_of_t_is_not_monomial|v_doesnt_exist]"
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
