#!/usr/bin/env bash
# Wrapper for test/tests/bcs/diffusion_flux_pp — replicates the TestHarness pipeline.
# Usage: bash run_test.sh [all|setup|diffusivity_err|run|verify|run_diffusivity_cv|verify_diffusivity_cv]

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

run_diffusivity_err() {
  run_expect_err "diffusivity_err" "Diffusivity 'kappa1' not a Foam volScalarField" \
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

run_diffusivity_cv() {
  step "run_diffusivity_cv: hippo-opt (diffusivity=Cv)"
  if srun --mpi=pmi2 -K -n 1 hippo-opt -i main.i \
    FoamBCs/T_flux/diffusivity='Cv' Postprocessors/T_flux/expression='t'; then
    echo "PASS (run_diffusivity_cv)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (run_diffusivity_cv): hippo-opt exited non-zero"
    FAIL=$((FAIL + 1))
  fi
}

run_verify_diffusivity_cv() {
  step "verify_diffusivity_cv: analytical comparison"
  if python3 -m pytest test.py -v; then
    echo "PASS (verify_diffusivity_cv)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (verify_diffusivity_cv)"
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
  run_diffusivity_err
  run_run
  run_verify
  run_diffusivity_cv
  run_verify_diffusivity_cv
  summary
}

MODE="${1:-all}"
case "$MODE" in
  all) run_all ;;
  setup) run_setup ;;
  diffusivity_err) run_diffusivity_err; summary ;;
  run) run_run ;;
  verify) run_verify ;;
  run_diffusivity_cv) run_diffusivity_cv ;;
  verify_diffusivity_cv) run_verify_diffusivity_cv ;;
  *)
    echo "Usage: $0 [all|setup|diffusivity_err|run|verify|run_diffusivity_cv|verify_diffusivity_cv]"
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
