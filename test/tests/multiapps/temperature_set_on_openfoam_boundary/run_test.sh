#!/usr/bin/env bash
# Wrapper for test/tests/multiapps/temperature_set_on_openfoam_boundary — mirrors tests file.
# Usage: bash run_test.sh [all|serial|parallel]
#        bash run_test.sh serial [setup|run|verify]
#        bash run_test.sh parallel [setup|run|reconstruct|verify]

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HIPPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
PYTHONPATH="${HIPPO_ROOT}/python:${PYTHONPATH:-}"
export PYTHONPATH

cd "${SCRIPT_DIR}"

step() { echo; echo "=== $* ==="; }

# foamCleanCase/Allclean's CleanFunctions are not available in all OpenFOAM
# installs (e.g. minimal/site builds without bin/tools/CleanFunctions).
# Reimplement the equivalent cleanup manually: drop generated time
# directories (keep the initial "0"), decomposed processor dirs, and other
# run artifacts.
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

run_serial_setup() {
  step "serial setup: clean_case + blockMesh"
  clean_case buoyantCavity
  blockMesh -case buoyantCavity
}

run_serial_run() {
  step "serial run: hippo-opt (serial)"
  if srun --mpi=pmi2 -K -n 1 hippo-opt -i run.i; then
    echo "PASS (serial run)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (serial run): hippo-opt exited non-zero"
    FAIL=$((FAIL + 1))
  fi
}

run_serial_verify() {
  step "serial verify: analytical comparison"
  if python3 -m pytest test.py -v; then
    echo "PASS (serial verify)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (serial verify)"
    FAIL=$((FAIL + 1))
  fi
}

run_serial_all() {
  run_serial_setup
  run_serial_run
  run_serial_verify
}

run_parallel_setup() {
  step "parallel setup: clean_case + blockMesh + decomposePar"
  clean_case buoyantCavity
  blockMesh -case buoyantCavity
  decomposePar -case buoyantCavity -force
}

run_parallel_run() {
  step "parallel run: hippo-opt (n=4)"
  if srun --mpi=pmi2 -K -n 4 hippo-opt -i run.i; then
    echo "PASS (parallel run)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (parallel run): hippo-opt exited non-zero"
    FAIL=$((FAIL + 1))
  fi
}

run_parallel_reconstruct() {
  step "parallel reconstruct: reconstructPar"
  if reconstructPar -case buoyantCavity; then
    echo "PASS (parallel reconstruct)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (parallel reconstruct)"
    FAIL=$((FAIL + 1))
  fi
}

run_parallel_verify() {
  step "parallel verify: analytical comparison"
  if python3 -m pytest test.py -v; then
    echo "PASS (parallel verify)"
    PASS=$((PASS + 1))
  else
    echo "FAILED (parallel verify)"
    FAIL=$((FAIL + 1))
  fi
}

run_parallel_all() {
  run_parallel_setup
  run_parallel_run
  run_parallel_reconstruct
  run_parallel_verify
}

summary() {
  echo
  echo "=== Summary: ${PASS} passed, ${FAIL} failed ==="
  [ "${FAIL}" -eq 0 ]
}

run_all() {
  run_serial_all
  run_parallel_all
  summary
}

MODE="${1:-all}"
SUBMODE="${2:-all}"
case "$MODE" in
  all) run_all ;;
  serial)
    case "$SUBMODE" in
      all) run_serial_all; summary ;;
      setup) run_serial_setup ;;
      run) run_serial_run; summary ;;
      verify) run_serial_verify; summary ;;
      *) echo "Usage: $0 serial [all|setup|run|verify]"; exit 1 ;;
    esac
    ;;
  parallel)
    case "$SUBMODE" in
      all) run_parallel_all; summary ;;
      setup) run_parallel_setup ;;
      run) run_parallel_run; summary ;;
      reconstruct) run_parallel_reconstruct; summary ;;
      verify) run_parallel_verify; summary ;;
      *) echo "Usage: $0 parallel [all|setup|run|reconstruct|verify]"; exit 1 ;;
    esac
    ;;
  *)
    echo "Usage: $0 [all|serial|parallel] [subcommand]"
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
