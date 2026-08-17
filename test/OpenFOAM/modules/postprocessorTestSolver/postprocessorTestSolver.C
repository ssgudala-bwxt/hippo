#include "DimensionedField.H"
#include "HippoSolverRegistry.h"
#include "dimensionSets.H"
#include "dimensionedScalar.H"
#include "dimensionedVector.H"
#include "fvcFlux.H"
#include "fvMesh.H"
#include "fvMeshMover.H"
#include "fvModels.H"
#include "postprocessorTestSolver.H"
#include "scalar.H"
#include "volFieldsFwd.H"
#include "volMesh.H"

namespace
{
Hippo::HippoSolver *
createPostprocessorTestSolver(Foam::fvMesh & mesh)
{
  return new Foam::solvers::postprocessorTestSolver(mesh);
}

[[maybe_unused]] const bool registered_postprocessor_test_solver =
    Hippo::registerSolverModule("postprocessorTestSolver", createPostprocessorTestSolver);
} // namespace

bool
Foam::solvers::postprocessorTestSolver::dependenciesModified() const
{
  return runTime().controlDict().modified();
}

bool
Foam::solvers::postprocessorTestSolver::read()
{
  maxDeltaT_ = runTime().controlDict().found("maxDeltaT")
                   ? runTime().controlDict().lookup<scalar>("maxDeltaT", runTime().userUnits())
                   : vGreat;

  return true;
}

Foam::solvers::postprocessorTestSolver::postprocessorTestSolver(fvMesh & mesh)
  : Hippo::HippoSolver(mesh),
    maxDeltaT_(vGreat),
    thermoPtr_(fluidThermo::New(mesh)),
    thermo_(thermoPtr_()),
    U_(IOobject("U", mesh.time().name(), mesh, IOobject::MUST_READ, IOobject::AUTO_WRITE), mesh),
    p_(IOobject("p", mesh.time().name(), mesh, IOobject::MUST_READ, IOobject::AUTO_WRITE), mesh),
    p_rgh_(IOobject("p_rgh", mesh.time().name(), mesh, IOobject::MUST_READ, IOobject::AUTO_WRITE),
           mesh),
    rho_(IOobject("rho", mesh.time().name(), mesh, IOobject::MUST_READ, IOobject::AUTO_WRITE), mesh),
    phi_(IOobject("phi", mesh.time().name(), mesh, IOobject::NO_READ, IOobject::AUTO_WRITE),
         fvc::flux(U_)),
    turbulence_(compressible::momentumTransportModel::New(rho_, U_, phi_, thermo_)),
    thermophysicalTransport_(fluidThermophysicalTransportModel::New(turbulence_(), thermo_)),
    pimple_(mesh)
{
  read();
}

Foam::scalar
Foam::solvers::postprocessorTestSolver::maxDeltaT() const
{
  return min(Foam::fvModels::New(mesh()).maxDeltaT(), maxDeltaT_);
}

void
Foam::solvers::postprocessorTestSolver::preSolve()
{
  if (dependenciesModified())
    read();

  Foam::fvModels::New(mesh()).preUpdateMesh();
  mesh().update();
}

void
Foam::solvers::postprocessorTestSolver::moveMeshIfNeeded()
{
  if (pimple_.firstIter() || pimple_.moveMeshOuterCorrectors())
  {
    if (!mesh().mover().solidBody())
      FatalErrorInFunction
          << "Solver postprocessorTestSolver does not support non-solid body mesh motion"
          << exit(FatalError);

    mesh().move();
  }
}

void
Foam::solvers::postprocessorTestSolver::thermophysicalPredictor()
{
  volScalarField & h = thermo_.he();
  const volScalarField & Cp = thermo_.Cp();

  volScalarField t(IOobject("0", "0", mesh()),
                   mesh(),
                   dimTemperature,
                   runTime().userTimeValue() * mesh().C().component(0)->internalField(),
                   runTime().userTimeValue() * mesh().C().component(0)->boundaryField());
  h = Cp * t;

  thermo_.correct();
}

void
Foam::solvers::postprocessorTestSolver::solve()
{
  while (pimple_.loop())
  {
    moveMeshIfNeeded();
    thermophysicalPredictor();
  }
}
