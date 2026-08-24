#pragma once

#include "HippoSolver.h"
#include "fvCFD.H"
#include "fluidThermo.H"
#include "pimpleControl.H"

namespace Foam
{
namespace solvers
{

// Postprocessor test solver: only reads/holds U and rho fields to exercise
// FoamBC/FoamPostprocessor coupling (e.g. FoamMassFlowRateInletBC,
// FoamSideAdvectiveFluxIntegral). Deliberately avoids fluidThermo/turbulence
// dependencies since minimal OpenFOAM installs may not ship those libraries.
class postprocessorTestSolver : public Hippo::HippoSolver
{
protected:
  scalar maxDeltaT_;
  volVectorField U_;
  volScalarField rho_;
  volScalarField T_;
  autoPtr<fluidThermo> pThermo_;
  pimpleControl pimple_;

  bool read();

public:
  explicit postprocessorTestSolver(fvMesh & mesh);
  postprocessorTestSolver(const postprocessorTestSolver &) = delete;
  ~postprocessorTestSolver() override = default;

  scalar maxDeltaT() const override;
  void preSolve() override;
  void moveMesh() override {}
  void solve() override;

  void operator=(const postprocessorTestSolver &) = delete;
};

} // namespace solvers
} // namespace Foam
