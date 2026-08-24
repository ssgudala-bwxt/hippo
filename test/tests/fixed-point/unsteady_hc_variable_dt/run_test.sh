#!/usr/bin/env bash
# Wrapper for test/tests/fixed-point/unsteady_hc_variable_dt — replicates
# the TestHarness pipeline (tests file: run_reference/{setup,run,copy},
# unsteady_1d_variable_dt/{setup,run(exodiff),verify,prep_clean}).
# Usage: bash run_test.sh [all|run_reference|run|verify|clean_gold]

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HIPPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
PYTHONPATH="${HIPPO_ROOT}/python:${PYTHONPATH:-}"
export PYTHONPATH

cd "${SCRIPT_DIR}"

step() { echo; echo "=== $* ==="; }

PASS=0
FAIL=0

# The reference run uses fixed_point_max_its=1 (single fixed-point iteration
# per timestep) — this is the "gold" behavior against which the default
# (multi-iteration) run below must reproduce exactly (both the MOOSE solid
# main_out.e and the OpenFOAM foam/ case results).
run_run_reference() {
  step "run_reference: setup + run (1 fixed-point iteration) + copy to gold/"
  (cd foam && ./Allclean && blockMesh)
  if srun --mpi=pmi2 -K -n 1 hippo-opt -i main.i Executioner/fixed_point_max_its=1; then
    echo "PASS (run_reference run)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (run_reference run): hippo-opt exited non-zero"
    FAIL=$((FAIL + 1))
    return
  fi
  mkdir -p gold
  cp -r foam/* gold/
  cp main_out.e gold/
  echo "PASS (run_reference copy)"
  PASS=$((PASS + 1))
}

run_run() {
  step "unsteady_1d_variable_dt: setup + run (default fixed-point its) + exodiff"
  (cd foam && ./Allclean && blockMesh)
  if srun --mpi=pmi2 -K -n 1 hippo-opt -i main.i; then
    echo "PASS (run)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (run): hippo-opt exited non-zero"
    FAIL=$((FAIL + 1))
    return
  fi
  # KNOWN LIMITATION: this case uses ddtSchemes { default CrankNicolson 1; }
  # in foam/system/fvSchemes. Crank-Nicolson + fixed-point iteration has a
  # documented accuracy loss (see the comment on removeOldTime() in
  # include/mesh/FoamDataStore.h), so the multi-iteration (default) run
  # drifts from the single-iteration gold/ reference. This is expected until
  # that limitation is addressed in Hippo's core fixed-point restore logic.
  if exodiff main_out.e gold/main_out.e; then
    echo "PASS (exodiff)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (exodiff) [EXPECTED: known Crank-Nicolson + fixed-point limitation, see FoamDataStore.h removeOldTime() comment]"
    FAIL=$((FAIL + 1))
  fi
}

run_verify() {
  step "verify: compare solid/fluid fields against gold/ and analytical solution"
  # KNOWN LIMITATION: all three sub-tests (test_solid_fixed_point,
  # test_fluid_fixed_point, test_analytical) are expected to fail here due to
  # the Crank-Nicolson + fixed-point accuracy loss documented in
  # FoamDataStore.h's removeOldTime() comment. Thresholds are kept at their
  # original values (not loosened) to make this limitation visible rather
  # than papered over.
  if python3 -m pytest test.py -v; then
    echo "PASS (verify)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (verify) [EXPECTED: known Crank-Nicolson + fixed-point limitation, see FoamDataStore.h removeOldTime() comment]"
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
  run_run_reference
  run_run
  run_verify
  run_clean_gold
  summary
}

MODE="${1:-all}"
case "$MODE" in
  all) run_all ;;
  run_reference) run_run_reference ;;
  run) run_run ;;
  verify) run_verify ;;
  clean_gold) run_clean_gold ;;
  *)
    echo "Usage: $0 [all|run_reference|run|verify|clean_gold]"
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
