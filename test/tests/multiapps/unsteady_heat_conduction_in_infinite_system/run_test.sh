#!/usr/bin/env bash
# Wrapper for test/tests/multiapps/unsteady_heat_conduction_in_infinite_system
# Usage: bash run_test.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HIPPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
PYTHONPATH="${HIPPO_ROOT}/python:${PYTHONPATH:-}"
export PYTHONPATH

cd "${SCRIPT_DIR}"

step() { echo; echo "=== $* ==="; }

step "setup: blockMesh + decomposePar"
cd fluid-openfoam
./Allclean
blockMesh
decomposePar
cd ..

step "run: hippo-opt (n=2)"
srun --mpi=pmi2 -K -n 2 hippo-opt -i run.i --allow-test-objects

step "reconstruct"
reconstructPar -case fluid-openfoam

step "verify: analytical comparison"
python3 -m pytest test.py -v

echo
echo "=== ALL STEPS PASSED ==="
