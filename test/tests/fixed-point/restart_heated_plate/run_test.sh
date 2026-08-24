#!/usr/bin/env bash
# Wrapper for test/tests/fixed-point/restart_heated_plate — replicates the
# TestHarness pipeline (tests file: heated_plate_restart/{setup,first,
# restart(exodiff vs ../heated_plate_converge gold),prep_verify,verify}).
#
# NOTE: this test compares against ../heated_plate_converge/fluid-openfoam,
# so `bash ../heated_plate_converge/run_test.sh all` must have been run
# first (and left its fluid-openfoam/ output in place) before `verify` here
# will succeed.
# Usage: bash run_test.sh [all|setup|first|restart|prep_verify|verify]

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
  clean_case fluid-openfoam
  blockMesh -case fluid-openfoam
  decomposePar -force -case fluid-openfoam
}

run_first() {
  step "first: hippo-opt (n=4) --test-checkpoint-half-transient"
  if srun --mpi=pmi2 -K -n 4 hippo-opt -i heated_plate.i --test-checkpoint-half-transient; then
    echo "PASS (first)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (first): hippo-opt exited non-zero"
    FAIL=$((FAIL + 1))
  fi
}

run_restart() {
  step "restart: hippo-opt (n=4) --recover heated_plate_out_cp/LATEST + exodiff"
  if srun --mpi=pmi2 -K -n 4 hippo-opt -i heated_plate.i --recover heated_plate_out_cp/LATEST; then
    echo "PASS (restart)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (restart): hippo-opt exited non-zero"
    FAIL=$((FAIL + 1))
    return
  fi
  # tests file: gold_dir = '../heated_plate_converge' (compare against that
  # test's regression gold, not a local gold/ dir).
  if exodiff heated_plate_out.e "${SCRIPT_DIR}/../heated_plate_converge/gold/heated_plate_out.e"; then
    echo "PASS (exodiff)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (exodiff)"
    FAIL=$((FAIL + 1))
  fi
}

run_prep_verify() {
  step "prep_verify: reconstructPar"
  reconstructPar -case fluid-openfoam
}

run_verify() {
  step "verify: compare fluid-openfoam/ against ../heated_plate_converge/fluid-openfoam"
  if python3 -m pytest test.py -v; then
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
  run_first
  run_restart
  run_prep_verify
  run_verify
  summary
}

MODE="${1:-all}"
case "$MODE" in
  all) run_all ;;
  setup) run_setup ;;
  first) run_first ;;
  restart) run_restart ;;
  prep_verify) run_prep_verify ;;
  verify) run_verify ;;
  *)
    echo "Usage: $0 [all|setup|first|restart|prep_verify|verify]"
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
