#pragma once

#include "HippoSolver.h"
#include "fvCFD.H"
#include "pimpleControl.H"

namespace Foam
{
namespace solvers
{

// Simplified heat-transfer test solver.  Uses a direct fvScalarMatrix Laplacian
// on a pre-existing T field instead of solidThermophysicalTransportModel, which
// is not available in this ESI v2606 system deployment.
class transferTestSolver : public Hippo::HippoSolver
{
protected:
  scalar maxDeltaT_;
  volScalarField T_;
  dimensionedScalar kappa_; // thermal diffusivity [m^2/s]
  pimpleControl pimple_;

  bool dependenciesModified() const;
  bool read();

public:
  const volScalarField & T;

  explicit transferTestSolver(fvMesh & mesh);
  transferTestSolver(const transferTestSolver &) = delete;
  ~transferTestSolver() override = default;

  scalar maxDeltaT() const override;
  void preSolve() override;
  void moveMesh() override {}
  void solve() override;

  void operator=(const transferTestSolver &) = delete;
};

} // namespace solvers
} // namespace Foam
