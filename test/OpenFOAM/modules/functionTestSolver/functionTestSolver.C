#include "functionTestSolver.H"
#include "HippoSolverRegistry.h"
#include "fvMeshMover.H"
#include "fvModels.H"
#include "fvcDdt.H"

namespace
{
Hippo::HippoSolver *
createFunctionTestSolver(Foam::fvMesh & mesh)
{
  return new Foam::solvers::functionTestSolver(mesh);
}

[[maybe_unused]] const bool registered_function_test_solver =
    Hippo::registerSolverModule("functionTestSolver", createFunctionTestSolver);
} // namespace

bool
Foam::solvers::functionTestSolver::dependenciesModified() const
{
  return runTime().controlDict().modified();
}

bool
Foam::solvers::functionTestSolver::read()
{
  maxDeltaT_ = runTime().controlDict().found("maxDeltaT")
                   ? runTime().controlDict().lookup<scalar>("maxDeltaT", runTime().userUnits())
                   : vGreat;

  return true;
}

Foam::solvers::functionTestSolver::functionTestSolver(fvMesh & mesh)
  : Hippo::HippoSolver(mesh),
    maxDeltaT_(vGreat),
    T_(IOobject("T", mesh.time().name(), mesh, IOobject::NO_READ, IOobject::AUTO_WRITE), mesh),
    dTdt_(IOobject("dTdt", mesh.time().name(), mesh, IOobject::NO_READ, IOobject::AUTO_WRITE),
          mesh,
          dimensionedScalar{dimTemperature / dimTime, 0.}),
    kappa_(IOobject("kappa", mesh.time().name(), mesh, IOobject::NO_READ, IOobject::AUTO_WRITE),
           mesh,
           1.),
    pimple_(mesh),
    T(T_),
    dTdt(dTdt_),
    kappa(kappa_)
{
  read();
}

Foam::scalar
Foam::solvers::functionTestSolver::maxDeltaT() const
{
  return min(Foam::fvModels::New(mesh()).maxDeltaT(), maxDeltaT_);
}

void
Foam::solvers::functionTestSolver::preSolve()
{
  if (dependenciesModified())
    read();

  Foam::fvModels::New(mesh()).preUpdateMesh();
  mesh().update();
}

void
Foam::solvers::functionTestSolver::moveMeshIfNeeded()
{
  if (pimple_.firstIter() || pimple_.moveMeshOuterCorrectors())
  {
    if (!mesh().mover().solidBody())
      FatalErrorInFunction << "Solver functionTestSolver does not support non-solid body mesh motion"
                           << exit(FatalError);

    mesh().move();
  }
}

void
Foam::solvers::functionTestSolver::thermophysicalPredictor()
{
  dimensioned<Foam::scalar> T0("T0", dimTemperature, mesh().time().userTimeValue());
  T_ = T0;
  dTdt_ = fvc::ddt(T_);
}

void
Foam::solvers::functionTestSolver::solve()
{
  while (pimple_.loop())
  {
    moveMeshIfNeeded();
    thermophysicalPredictor();
  }
}
