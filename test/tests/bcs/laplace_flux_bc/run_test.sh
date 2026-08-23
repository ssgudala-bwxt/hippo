#!/usr/bin/env bash
# Wrapper for test/tests/bcs/laplace_flux_bc — replicates the TestHarness pipeline.
# Usage: bash run_test.sh [all|setup|check_invalid_diffusivity|run|verify|run_diffusivity|verify_diffusivity]

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

run_check_invalid_diffusivity() {
  run_expect_err "check_invalid_diffusivity" "Diffusivity 'kappa1' is neither a Foam volScalarField nor a scalar" \
    FoamBCs/T_flux/diffusivity=kappa1
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

run_verify() {
  step "verify: analytical comparison"
  if python3 -m pytest test.py -v; then
    echo "PASS (verify)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (verify)"
    FAIL=$((FAIL + 1))
  fi
}

run_diffusivity() {
  step "run_diffusivity: hippo-opt (diffusivity=Cv)"
  if srun --mpi=pmi2 -K -n 1 hippo-opt -i main.i \
    FoamBCs/T_flux/diffusivity=Cv AuxKernels/T_flux/expression=t; then
    echo "PASS (run_diffusivity)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (run_diffusivity): hippo-opt exited non-zero"
    FAIL=$((FAIL + 1))
  fi
}

run_verify_diffusivity() {
  step "verify_diffusivity: analytical comparison"
  if python3 -m pytest test.py -v; then
    echo "PASS (verify_diffusivity)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (verify_diffusivity)"
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
  run_check_invalid_diffusivity
  run_run
  run_verify
  run_diffusivity
  run_verify_diffusivity
  summary
}

MODE="${1:-all}"
case "$MODE" in
  all) run_all ;;
  setup) run_setup ;;
  check_invalid_diffusivity) run_check_invalid_diffusivity; summary ;;
  run) run_run ;;
  verify) run_verify ;;
  run_diffusivity) run_diffusivity ;;
  verify_diffusivity) run_verify_diffusivity ;;
  *)
    echo "Usage: $0 [all|setup|check_invalid_diffusivity|run|verify|run_diffusivity|verify_diffusivity]"
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
