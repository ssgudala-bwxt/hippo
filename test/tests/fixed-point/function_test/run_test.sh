#!/usr/bin/env bash
# Wrapper for test/tests/fixed-point/function_test — replicates the
# TestHarness pipeline (tests file: create_reference/{setup,run,copy},
# function_test/{setup,run,verify,prep_clean}).
# Usage: bash run_test.sh [all|create_reference|run|verify|clean_gold]

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

# Produces gold/ by running with a single fixed-point iteration
# (fixed_point_min_its=1 fixed_point_max_its=1), the reference behavior
# against which the default (2-iteration) run is compared.
run_create_reference() {
  step "create_reference: setup + run (1 fixed-point iteration) + copy to gold/"
  clean_case foam
  blockMesh -case foam
  if srun --mpi=pmi2 -K -n 1 hippo-opt -i main.i \
    Executioner/fixed_point_min_its=1 Executioner/fixed_point_max_its=1; then
    echo "PASS (create_reference run)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (create_reference run): hippo-opt exited non-zero"
    FAIL=$((FAIL + 1))
    return
  fi
  mkdir -p gold
  cp main_out.e gold/main_out.e
  cp -r foam/* gold/
  echo "PASS (create_reference copy)"
  PASS=$((PASS + 1))
}

run_run() {
  step "function_test setup + run (2 fixed-point iterations, default)"
  clean_case foam
  blockMesh -case foam
  if srun --mpi=pmi2 -K -n 1 hippo-opt -i main.i; then
    echo "PASS (run)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (run): hippo-opt exited non-zero"
    FAIL=$((FAIL + 1))
  fi
}

run_verify() {
  step "verify: compare T/dTdt against analytic values and gold/"
  if python3 -m pytest test.py -v; then
    echo "PASS (verify)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (verify)"
    FAIL=$((FAIL + 1))
  fi
}

run_clean_gold() {
  step "prep_clean: remove gold/"
  rm -rf gold
}

summary() {
  echo
  echo "=== Summary: ${PASS} passed, ${FAIL} failed ==="
  [ "${FAIL}" -eq 0 ]
}

run_all() {
  run_create_reference
  run_run
  run_verify
  run_clean_gold
  summary
}

MODE="${1:-all}"
case "$MODE" in
  all) run_all ;;
  create_reference) run_create_reference ;;
  run) run_run ;;
  verify) run_verify ;;
  clean_gold) run_clean_gold ;;
  *)
    echo "Usage: $0 [all|create_reference|run|verify|clean_gold]"
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
