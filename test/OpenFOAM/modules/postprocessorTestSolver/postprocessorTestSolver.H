#pragma once

#include "HippoSolver.h"

#include "compressibleMomentumTransportModel.H"
#include "fluidThermo.H"
#include "fluidThermophysicalTransportModel.H"
#include "surfaceFields.H"
#include "volFields.H"

namespace Foam
{
namespace solvers
{

class postprocessorTestSolver : public Hippo::HippoSolver
{
protected:
  scalar maxDeltaT_;
  autoPtr<fluidThermo> thermoPtr_;
  fluidThermo & thermo_;
  volVectorField U_;
  volScalarField p_;
  volScalarField p_rgh_;
  volScalarField rho_;
  surfaceScalarField phi_;
  autoPtr<compressible::momentumTransportModel> turbulence_;
  autoPtr<fluidThermophysicalTransportModel> thermophysicalTransport_;
  pimpleControl pimple_;

  bool dependenciesModified() const;
  bool read();
  void moveMeshIfNeeded();
  void thermophysicalPredictor();

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
