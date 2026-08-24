#pragma once

#include "HippoSolver.h"
#include "fvCFD.H"
#include "pimpleControl.H"
#include "solidThermo.H"

namespace Foam
{
namespace solvers
{

// Simplified heat-transfer test solver. Rather than solving a real diffusion
// PDE, it directly imposes an analytic internal-energy profile via a
// solidThermo model (constructed from constant/thermophysicalProperties) so
// that T follows a known closed-form solution each timestep. The solidThermo
// model is registered in the mesh object registry, which also lets
// functionObjects such as wallHeatFlux (which fall back to solidThermo when
// no turbulence/fluidThermo model is present) compute alpha()/he() from it.
class transferTestSolver : public Hippo::HippoSolver
{
protected:
  scalar maxDeltaT_;
  volScalarField T_;
  autoPtr<solidThermo> pThermo_;
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
