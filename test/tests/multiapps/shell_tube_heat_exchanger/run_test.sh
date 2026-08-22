#!/usr/bin/env bash
# Wrapper for test/tests/multiapps/shell_tube_heat_exchanger
# Usage: bash run_test.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HIPPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
PYTHONPATH="${HIPPO_ROOT}/python:${PYTHONPATH:-}"
export PYTHONPATH

cd "${SCRIPT_DIR}"

step() { echo; echo "=== $* ==="; }

step "setup: clean + extract meshes + decomposePar"
# Clean only processor dirs from previous runs
rm -rf fluid_inner/processor* fluid_outer/processor*

echo "Extracting the Inner-Fluid mesh..."
rm -rf fluid_inner/constant/polyMesh
tar -xvf fluid_inner_mesh.tar.gz
mv polyMesh.org fluid_inner/constant/polyMesh
gzip -d -q fluid_inner/constant/polyMesh/*

echo "Extracting the Outer-Fluid mesh..."
rm -rf fluid_outer/constant/polyMesh
tar -xvf fluid_outer_mesh.tar.gz
mv polyMesh.org fluid_outer/constant/polyMesh
gzip -d -q fluid_outer/constant/polyMesh/*

echo "Update boundary type"
sed -i 's/mapped/wall/g' fluid_inner/constant/polyMesh/boundary
sed -i 's/mapped/wall/g' fluid_outer/constant/polyMesh/boundary

decomposePar -case fluid_inner -force
decomposePar -case fluid_outer -force

step "run: hippo-opt (n=2)"
srun --mpi=pmi2 -K -n 2 hippo-opt -i solid.i --allow-test-objects

step "verify: exodiff"
exodiff -f "${SCRIPT_DIR}/exodiff_filter.txt" "${SCRIPT_DIR}/gold/solid_out.e" solid_out.e

step "cleanup"
rm -rf fluid_inner/processor* fluid_outer/processor*

echo
echo "=== ALL STEPS PASSED ==="
