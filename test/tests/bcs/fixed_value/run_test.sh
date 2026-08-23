#!/usr/bin/env bash
# Wrapper for test/tests/bcs/fixed_value — replicates the TestHarness pipeline.
# Usage: bash run_test.sh [serial|parallel|all]
#   serial   setup + run + verify (n=1)
#   parallel setup + run_parallel (n=4) + reconstruct + verify_parallel
#   all      runs serial, then parallel

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HIPPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
PYTHONPATH="${HIPPO_ROOT}/python:${PYTHONPATH:-}"
export PYTHONPATH

MODE="${1:-serial}"

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

run_setup() {
  step "setup: clean_case + blockMesh + decomposePar"
  clean_case foam
  blockMesh -case foam
  decomposePar -force -case foam
}

run_serial() {
  step "run: hippo-opt (serial)"
  srun --mpi=pmi2 -K -n 1 hippo-opt -i main.i
}

run_verify() {
  step "verify: analytical comparison"
  python3 -m pytest test.py -v
}

run_parallel() {
  step "run_parallel: hippo-opt (n=4)"
  srun --mpi=pmi2 -K -n 4 hippo-opt -i main.i
  step "reconstruct"
  reconstructPar -case foam
}

case "$MODE" in
  serial)
    run_setup
    run_serial
    run_verify
    ;;
  parallel)
    run_setup
    run_parallel
    run_verify
    ;;
  all)
    run_setup
    run_serial
    run_verify
    run_parallel
    run_verify
    ;;
  *)
    echo "Usage: $0 [serial|parallel|all]"
    exit 1
    ;;
esac

echo
echo "=== ALL STEPS PASSED ==="
