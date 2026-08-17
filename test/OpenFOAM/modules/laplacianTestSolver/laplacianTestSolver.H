#pragma once

#include "HippoSolver.h"

namespace Foam
{
namespace solvers
{

class laplacianTestSolver : public Hippo::HippoSolver
{
protected:
  scalar maxDeltaT_;
  volScalarField T_;
  volScalarField kappa_;
  pimpleControl pimple_;

  bool dependenciesModified() const;
  bool read();
  void moveMeshIfNeeded();
  void thermophysicalPredictor();

public:
  const volScalarField & T;
  const volScalarField & kappa;

  explicit laplacianTestSolver(fvMesh & mesh);
  laplacianTestSolver(const laplacianTestSolver &) = delete;
  ~laplacianTestSolver() override = default;

  scalar maxDeltaT() const override;
  void preSolve() override;
  void moveMesh() override {}
  void solve() override;

  void operator=(const laplacianTestSolver &) = delete;
};

} // namespace solvers
} // namespace Foam
