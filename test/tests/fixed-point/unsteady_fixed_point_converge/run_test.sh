#!/usr/bin/env bash
# Wrapper for test/tests/fixed-point/unsteady_fixed_point_converge — replicates
# the TestHarness pipeline (tests file: unsteady_converge/{setup,run(exodiff),
# prep_verify,verify,clean}).
# Usage: bash run_test.sh [all|setup|run|prep_verify|verify|clean]

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
}

run_run() {
  step "run: hippo-opt (n=2) + exodiff"
  if srun --mpi=pmi2 -K -n 2 hippo-opt -i main.i; then
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
  step "prep_verify: reconstructPar + unpack gold reference tarball"
  reconstructPar -case foam
  tar xvf gold/unsteady_fixed_point_reference.tar.gz -C gold/
}

run_verify() {
  step "verify: compare foam/ against gold/ OpenFOAM reference data"
  if python3 -m pytest test.py -v; then
    echo "PASS (verify)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (verify)"
    FAIL=$((FAIL + 1))
  fi
}

run_clean() {
  step "clean: remove unpacked gold reference case artifacts"
  (cd gold && ./Allclean && rm -rf 0 Allclean constant system)
}

summary() {
  echo
  echo "=== Summary: ${PASS} passed, ${FAIL} failed ==="
  [ "${FAIL}" -eq 0 ]
}

run_all() {
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
  setup) run_setup ;;
  run) run_run ;;
  prep_verify) run_prep_verify ;;
  verify) run_verify ;;
  clean) run_clean ;;
  *)
    echo "Usage: $0 [all|setup|run|prep_verify|verify|clean]"
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
