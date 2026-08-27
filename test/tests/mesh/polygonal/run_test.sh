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

# foamCleanCase / Allclean rely on $WM_PROJECT_DIR/bin/tools/CleanFunctions,
# which is not present in all OpenFOAM installs (e.g. minimal/site builds).
# Reimplement the equivalent cleanup manually: drop the generated mesh,
# generated time directories (keep the initial "0"), decomposed processor
# dirs, and other run artifacts.
#
# The initial "0" directory is additionally restored from git when available.
# Solvers write fields back into "0", so a previous run can leave a
# nonuniform field there sized for the *old* mesh. Any setup that changes the
# cell count afterwards (notably polyDualMesh in the polygonal test) then
# fails with "Size <n> is not equal to the expected length <m>".
clean_case() {
  local dir="$1"
  rm -rf "${dir}"/constant/polyMesh "${dir}"/constant/dualMesh "${dir}"/processor* \
         "${dir}"/postProcessing "${dir}"/VTK "${dir}"/dynamicCode "${dir}"/probes \
         "${dir}"/*.obj "${dir}"/*.foam "${dir}"/log.*
  local d
  for d in "${dir}"/[0-9]*; do
    [ -e "$d" ] || continue
    [ "$(basename "$d")" = "0" ] && continue
    rm -rf "$d"
  done
  if git -C "${SCRIPT_DIR}" rev-parse --git-dir >/dev/null 2>&1; then
    git -C "${SCRIPT_DIR}" checkout -- "${dir}/0" 2>/dev/null || true
  fi
}

run_setup() {
  step "setup: fluentMeshToFoam + decomposePar"
  clean_case fluid-openfoam
  cd fluid-openfoam
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
