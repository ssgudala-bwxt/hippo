#!/usr/bin/env bash
# Wrapper for test/tests/multiapps/simplified_heat_exchanger
# Usage: bash run_test.sh [parallel|serial]
#   parallel  blockMesh + decomposePar (n=4), default
#   serial    blockMesh only (n=1), no decomposition — for debugging

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HIPPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
PYTHONPATH="${HIPPO_ROOT}/python:${PYTHONPATH:-}"
export PYTHONPATH

MODE="${1:-serial}"

cd "${SCRIPT_DIR}"

step() { echo; echo "=== $* ==="; }

setup_parallel() {
  step "setup: blockMesh + decomposePar (n=4) for both fluid cases"
  # ESI OpenFOAM uses physicalProperties/momentumTransport;
  # older versions expected thermophysicalProperties/turbulenceProperties
  ln -sf physicalProperties fluid-top-openfoam/constant/thermophysicalProperties 2>/dev/null || true
  ln -sf physicalProperties fluid-bottom-openfoam/constant/thermophysicalProperties 2>/dev/null || true
  ln -sf momentumTransport fluid-top-openfoam/constant/turbulenceProperties 2>/dev/null || true
  ln -sf momentumTransport fluid-bottom-openfoam/constant/turbulenceProperties 2>/dev/null || true
  sed -i 's/numberOfSubdomains [0-9]*/numberOfSubdomains 4/' \
    fluid-top-openfoam/system/decomposeParDict \
    fluid-bottom-openfoam/system/decomposeParDict
  sed -i 's/n           ([0-9]* 1 1)/n           (4 1 1)/' \
    fluid-top-openfoam/system/decomposeParDict \
    fluid-bottom-openfoam/system/decomposeParDict
  (cd fluid-top-openfoam && rm -rf processor* && blockMesh && decomposePar)
  for d in fluid-top-openfoam/processor*/; do cp -r fluid-top-openfoam/constant/. "${d}constant/"; done
  (cd fluid-bottom-openfoam && rm -rf processor* && blockMesh && decomposePar)
  for d in fluid-bottom-openfoam/processor*/; do cp -r fluid-bottom-openfoam/constant/. "${d}constant/"; done
}

setup_serial() {
  step "setup: blockMesh only (serial, no decomposition)"
  # ESI OpenFOAM uses physicalProperties/momentumTransport;
  # older versions expected thermophysicalProperties/turbulenceProperties
  ln -sf physicalProperties fluid-top-openfoam/constant/thermophysicalProperties 2>/dev/null || true
  ln -sf physicalProperties fluid-bottom-openfoam/constant/thermophysicalProperties 2>/dev/null || true
  ln -sf momentumTransport fluid-top-openfoam/constant/turbulenceProperties 2>/dev/null || true
  ln -sf momentumTransport fluid-bottom-openfoam/constant/turbulenceProperties 2>/dev/null || true
  sed -i 's/numberOfSubdomains [0-9]*/numberOfSubdomains 1/' \
    fluid-top-openfoam/system/decomposeParDict \
    fluid-bottom-openfoam/system/decomposeParDict
  sed -i 's/n           ([0-9]* 1 1)/n           (1 1 1)/' \
    fluid-top-openfoam/system/decomposeParDict \
    fluid-bottom-openfoam/system/decomposeParDict
  (cd fluid-top-openfoam && rm -rf processor* && blockMesh)
  (cd fluid-bottom-openfoam && rm -rf processor* && blockMesh)
}

case "$MODE" in
  parallel)
    setup_parallel
    step "run: hippo-opt (n=4)"
    srun --mpi=pmi2 -K -n 4 hippo-opt -i solid.i --allow-test-objects
    step "reconstruct"
    reconstructPar -case fluid-top-openfoam -latestTime
    reconstructPar -case fluid-bottom-openfoam -latestTime
    ;;
  serial)
    setup_serial
    step "run: hippo-opt (serial)"
    srun --mpi=pmi2 -K -n 1 hippo-opt -i solid.i --allow-test-objects
    ;;
  *)
    echo "Usage: $0 [parallel|serial]"
    exit 1
    ;;
esac

step "verify: analytical comparison"
python3 -m pytest test.py -v

echo
echo "=== ALL STEPS PASSED ==="
