#pragma once

#include "HippoSolver.h"

namespace Foam
{
namespace solvers
{

class odeTestSolver : public Hippo::HippoSolver
{
protected:
  scalar maxDeltaT_;
  volScalarField T_;
  volScalarField kappa_;
  pimpleControl pimple_;

  bool read();

public:
  const volScalarField & T;
  const volScalarField & kappa;

  explicit odeTestSolver(fvMesh & mesh);
  odeTestSolver(const odeTestSolver &) = delete;
  ~odeTestSolver() override = default;

  scalar maxDeltaT() const override;
  void preSolve() override;
  void moveMesh() override {}
  void solve() override;

  void operator=(const odeTestSolver &) = delete;
};

} // namespace solvers
} // namespace Foam
