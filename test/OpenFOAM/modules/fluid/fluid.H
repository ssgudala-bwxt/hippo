#pragma once

// HippoSolver wrapping buoyantPimpleFoam physics for ESI OpenFOAM-v2606.
// Uses static fvMesh (no dynamicFvMesh/mesh motion).
// No LTS, no MRF, optional radiation via run-time selection.

#include "HippoSolver.h"
#include "fvCFD.H"
#include "rhoThermo.H"
#include "turbulentFluidThermoModel.H"
#include "radiationModel.H"
#include "fvOptions.H"
#include "pimpleControl.H"
#include "pressureControl.H"
#include "IOMRFZoneList.H"

namespace Foam
{
namespace solvers
{

class fluid : public Hippo::HippoSolver
{
  // Thermophysics
  pimpleControl pimple_;
  autoPtr<rhoThermo> pThermo_;
  rhoThermo & thermo_;
  volScalarField & p_;
  volScalarField rho_;

  // Velocity & flux
  volVectorField U_;
  surfaceScalarField phi_;

  // Turbulence
  autoPtr<compressible::turbulenceModel> turbulence_;

  // Buoyancy
  uniformDimensionedVectorField g_;
  dimensionedScalar hRef_;
  dimensionedScalar ghRef_;
  volScalarField gh_;
  surfaceScalarField ghf_;

  // Pressure
  volScalarField p_rgh_;
  label pRefCell_;
  scalar pRefValue_;
  dimensionedScalar initialMass_;
  pressureControl pressureControl_;
  dimensionedScalar rhoMax_;
  dimensionedScalar rhoMin_;

  // Energy bookkeeping
  volScalarField dpdt_;
  volScalarField K_;

  // MRF / source terms / radiation
  IOMRFZoneList MRF_;
  autoPtr<radiation::radiationModel> radiation_;
  fv::options & fvOptions_;

public:
  explicit fluid(fvMesh & mesh);
  fluid(const fluid &) = delete;
  ~fluid() override = default;

  scalar maxDeltaT() const override { return 1e15; }
  bool transient() const override { return true; }
  void preSolve() override {}
  void moveMesh() override {}
  void solve() override;

  void operator=(const fluid &) = delete;
};

} // namespace solvers
} // namespace Foam
