#pragma once

#include "HippoSolver.h"
#include "fvCFD.H"
#include "pimpleControl.H"
#include "solidThermo.H"

namespace Foam
{
namespace solvers
{

// Real unsteady heat conduction test solver, used as an ESI replacement for
// the (nonexistent in ESI OpenFOAM) generic single-region "solid"
// Foam::solver module. ESI only ships a "solid" region solver as part of the
// multi-region chtMultiRegionFoam application (applications/solvers/
// heatTransfer/chtMultiRegionFoam/solid), which cannot be selected as a
// standalone Foam::solver::New() module the way "fluid" can. This module
// reimplements the same physics
// (rho*Cp*dT/dt = div(kappa*grad(T)), see solveSolid.H in that directory)
// for a single-region case via solidThermo, so tests originally written
// against `controlDict: solver solid;` can run unmodified against a
// `solver solidConductionTestSolver;` entry.
class solidConductionTestSolver : public Hippo::HippoSolver
{
protected:
  scalar maxDeltaT_;
  autoPtr<solidThermo> pThermo_;
  pimpleControl pimple_;

  bool read();

public:
  const volScalarField & T;

  explicit solidConductionTestSolver(fvMesh & mesh);
  solidConductionTestSolver(const solidConductionTestSolver &) = delete;
  ~solidConductionTestSolver() override = default;

  scalar maxDeltaT() const override;
  void preSolve() override;
  void moveMesh() override {}
  void solve() override;

  void operator=(const solidConductionTestSolver &) = delete;
};

} // namespace solvers
} // namespace Foam
