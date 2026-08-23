#!/usr/bin/env bash
# Wrapper for test/tests/mesh/polygonal — replicates the TestHarness pipeline.
# Usage: bash run_test.sh [serial|distributed|split|all]
#   serial      setup + run + verify (default)
#   distributed setup + run_distributed + reconstruct + verify_distributed
#   split       split_mesh + run_use_split + reconstruct_split + verify_split
#   all         runs serial, then distributed, then split in sequence

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HIPPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
PYTHONPATH="${HIPPO_ROOT}/python:${PYTHONPATH:-}"
export PYTHONPATH

MODE="${1:-serial}"

cd "${SCRIPT_DIR}"

step() { echo; echo "=== $* ==="; }

run_setup() {
  step "setup: fluentMeshToFoam + decomposePar"
  cd fluid-openfoam
  ./Allclean
  fluentMeshToFoam constant/polygon.msh
  polyDualMesh -overwrite 75
  decomposePar
  cd ..
}

run_serial() {
  step "run: hippo-opt (serial)"
  srun --mpi=pmi2 -K -n 1 hippo-opt -i run.i --allow-test-objects
}

run_verify() {
  step "verify: analytical comparison"
  python3 -m pytest test.py -v
}

run_distributed() {
  step "run_distributed: hippo-opt (n=2, --distributed-mesh)"
  srun --mpi=pmi2 -K -n 2 hippo-opt -i run.i --allow-test-objects --distributed-mesh
  step "reconstruct"
  reconstructPar -case fluid-openfoam
}

run_split() {
  step "split_mesh"
  srun --mpi=pmi2 -K -n 1 hippo-opt -i run.i --allow-test-objects --split-mesh 2 --split-file mesh_split
  step "run_use_split (n=2)"
  srun --mpi=pmi2 -K -n 2 hippo-opt -i run.i --allow-test-objects --use-split --split-file mesh_split
  step "reconstruct_split"
  reconstructPar -case fluid-openfoam
}

case "$MODE" in
  serial)
    run_setup
    run_serial
    run_verify
    ;;
  distributed)
    run_setup
    run_distributed
    run_verify
    ;;
  split)
    run_split
    run_verify
    ;;
  all)
    run_setup
    run_serial
    run_verify
    run_distributed
    run_verify
    run_split
    run_verify
    ;;
  *)
    echo "Usage: $0 [serial|distributed|split|all]"
    exit 1
    ;;
esac

echo
echo "=== ALL STEPS PASSED ==="
