#include "bcTestSolver.H"
#include "HippoSolverRegistry.h"
#include "dimensionSets.H"
#include "fvConstraints.H"
#include "fvMesh.H"
#include "fvMeshMover.H"
#include "fvModels.H"
#include "fvmLaplacian.H"

namespace
{
Hippo::HippoSolver *
createBcTestSolver(Foam::fvMesh & mesh)
{
  return new Foam::solvers::bcTestSolver(mesh);
}

[[maybe_unused]] const bool registered_bc_test_solver =
    Hippo::registerSolverModule("bcTestSolver", createBcTestSolver);
} // namespace

bool
Foam::solvers::bcTestSolver::dependenciesModified() const
{
  return runTime().controlDict().modified();
}

bool
Foam::solvers::bcTestSolver::read()
{
  maxDeltaT_ = runTime().controlDict().found("maxDeltaT")
                   ? runTime().controlDict().lookup<scalar>("maxDeltaT", runTime().userUnits())
                   : vGreat;

  return true;
}

Foam::solvers::bcTestSolver::bcTestSolver(fvMesh & mesh)
  : Hippo::HippoSolver(mesh),
    maxDeltaT_(vGreat),
    thermoPtr_(solidThermo::New(mesh)),
    thermo_(thermoPtr_()),
    T_(IOobject("T", mesh.time().name(), mesh, IOobject::NO_READ, IOobject::AUTO_WRITE), mesh),
    thermophysicalTransport_(solidThermophysicalTransportModel::New(thermo_)),
    pimple_(mesh),
    thermo(thermo_),
    T(T_)
{
  thermo.validate("solid", "h", "e");
  read();
}

Foam::scalar
Foam::solvers::bcTestSolver::maxDeltaT() const
{
  return min(Foam::fvModels::New(mesh()).maxDeltaT(), maxDeltaT_);
}

void
Foam::solvers::bcTestSolver::preSolve()
{
  if (dependenciesModified())
    read();

  Foam::fvModels::New(mesh()).preUpdateMesh();
  mesh().update();
}

void
Foam::solvers::bcTestSolver::moveMeshIfNeeded()
{
  if (pimple_.firstIter() || pimple_.moveMeshOuterCorrectors())
  {
    if (!mesh().mover().solidBody())
      FatalErrorInFunction << "Solver bcTestSolver does not support non-solid body mesh motion"
                           << exit(FatalError);

    mesh().move();
  }
}

void
Foam::solvers::bcTestSolver::thermophysicalPredictor()
{
  fvScalarMatrix eEqn(fvm::laplacian(thermo_.kappa(), thermo.he()));

  eEqn.solve();

  thermo.he().write();
  thermo_.correct();
}

void
Foam::solvers::bcTestSolver::solve()
{
  while (pimple_.loop())
  {
    moveMeshIfNeeded();
    thermophysicalPredictor();
  }
}
