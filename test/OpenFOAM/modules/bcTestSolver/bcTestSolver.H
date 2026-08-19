#pragma once

#include "HippoSolver.h"
#include "fvCFD.H"
#include "pimpleControl.H"

namespace Foam
{
namespace solvers
{

// BC test solver: solves a Laplacian on T to exercise OpenFOAM boundary
// condition coupling. Uses only finiteVolume (no solidThermo/transport model).
class bcTestSolver : public Hippo::HippoSolver
{
protected:
  scalar maxDeltaT_;
  volScalarField T_;
  dimensionedScalar kappa_;
  pimpleControl pimple_;

  bool read();

public:
  const volScalarField & T;

  explicit bcTestSolver(fvMesh & mesh);
  bcTestSolver(const bcTestSolver &) = delete;
  ~bcTestSolver() override = default;

  scalar maxDeltaT() const override;
  void preSolve() override;
  void moveMesh() override {}
  void solve() override;

  void operator=(const bcTestSolver &) = delete;
};

} // namespace solvers
} // namespace Foam
