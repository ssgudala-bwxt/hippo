#pragma once

#include "HippoSolver.h"

namespace Foam
{
namespace solvers
{

class functionTestSolver : public Hippo::HippoSolver
{
protected:
  scalar maxDeltaT_;
  volScalarField T_;
  volScalarField dTdt_;
  volScalarField kappa_;
  pimpleControl pimple_;

  bool read();

public:
  const volScalarField & T;
  const volScalarField & dTdt;
  const volScalarField & kappa;

  explicit functionTestSolver(fvMesh & mesh);
  functionTestSolver(const functionTestSolver &) = delete;
  ~functionTestSolver() override = default;

  scalar maxDeltaT() const override;
  void preSolve() override;
  void moveMesh() override {}
  void solve() override;

  void operator=(const functionTestSolver &) = delete;
};

} // namespace solvers
} // namespace Foam
