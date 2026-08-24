#!/usr/bin/env bash
# Wrapper for test/tests/fixed-point/restep_fixed_point — replicates the
# TestHarness pipeline (tests file: run_reference/{setup,run,copy},
# restep/{setup,run(exodiff),prep_verify,verify,clean}).
# Usage: bash run_test.sh [all|run_reference|setup|run|prep_verify|verify|clean]

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

# Reference run: uses FEProblem + a fixed ConstantDT TimeStepper (no restep,
# no FoamTimeStepper) at the same dt the real run settles on after its first
# cutback, establishing the "gold" behavior the restep run must reproduce.
run_run_reference() {
  step "run_reference: setup + run (ConstantDT, no restep) + copy to gold/"
  (cd foam && ./Allclean && blockMesh && decomposePar)
  if srun --mpi=pmi2 -K -n 2 hippo-opt -i main_restep.i \
    Problem/type=FEProblem Executioner/TimeStepper/type=ConstantDT \
    Executioner/TimeStepper/dt=0.0003125 -w; then
    echo "PASS (run_reference run)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (run_reference run): hippo-opt exited non-zero"
    FAIL=$((FAIL + 1))
    return
  fi
  reconstructPar -case foam
  mkdir -p gold
  cp -r foam/* gold/
  cp main_out.e gold/
  echo "PASS (run_reference copy)"
  PASS=$((PASS + 1))
}

run_setup() {
  step "restep setup: clean_case + blockMesh + decomposePar"
  clean_case foam
  blockMesh -case foam
  decomposePar -force -case foam
}

run_run() {
  step "restep run: hippo-opt (n=2, restep after failed convergence) + exodiff"
  if srun --mpi=pmi2 -K -n 2 hippo-opt -i main_restep.i --allow-test-objects; then
    echo "PASS (run)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (run): hippo-opt exited non-zero"
    FAIL=$((FAIL + 1))
    return
  fi
  if exodiff main_out.e gold/main_out.e; then
    echo "PASS (exodiff)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (exodiff)"
    FAIL=$((FAIL + 1))
  fi
}

run_prep_verify() {
  step "prep_verify: reconstructPar"
  reconstructPar -case foam
}

run_verify() {
  step "verify: compare foam/ against gold/"
  if python3 -m pytest test.py -v; then
    echo "PASS (verify)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (verify)"
    FAIL=$((FAIL + 1))
  fi
}

run_clean() {
  step "clean: remove gold/"
  rm -rf gold
}

summary() {
  echo
  echo "=== Summary: ${PASS} passed, ${FAIL} failed ==="
  [ "${FAIL}" -eq 0 ]
}

run_all() {
  run_run_reference
  run_setup
  run_run
  run_prep_verify
  run_verify
  run_clean
  summary
}

MODE="${1:-all}"
case "$MODE" in
  all) run_all ;;
  run_reference) run_run_reference ;;
  setup) run_setup ;;
  run) run_run ;;
  prep_verify) run_prep_verify ;;
  verify) run_verify ;;
  clean) run_clean ;;
  *)
    echo "Usage: $0 [all|run_reference|setup|run|prep_verify|verify|clean]"
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
