#include "dimensionSets.H"
#include "fvMesh.H"
#include "fvMeshMover.H"
#include "fvModels.H"
#include "HippoSolverRegistry.h"
#include "transferTestSolver.H"

namespace
{
Hippo::HippoSolver *
createTransferTestSolver(Foam::fvMesh & mesh)
{
  return new Foam::solvers::transferTestSolver(mesh);
}

[[maybe_unused]] const bool registered_transfer_test_solver =
    Hippo::registerSolverModule("transferTestSolver", createTransferTestSolver);
} // namespace

bool
Foam::solvers::transferTestSolver::dependenciesModified() const
{
  return runTime().controlDict().modified();
}

bool
Foam::solvers::transferTestSolver::read()
{
  maxDeltaT_ = runTime().controlDict().found("maxDeltaT")
                   ? runTime().controlDict().lookup<scalar>("maxDeltaT", runTime().userUnits())
                   : vGreat;

  return true;
}

Foam::solvers::transferTestSolver::transferTestSolver(fvMesh & mesh)
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
Foam::solvers::transferTestSolver::maxDeltaT() const
{
  return min(Foam::fvModels::New(mesh()).maxDeltaT(), maxDeltaT_);
}

void
Foam::solvers::transferTestSolver::preSolve()
{
  if (dependenciesModified())
    read();

  Foam::fvModels::New(mesh()).preUpdateMesh();
  mesh().update();
}

void
Foam::solvers::transferTestSolver::moveMeshIfNeeded()
{
  if (pimple_.firstIter() || pimple_.moveMeshOuterCorrectors())
  {
    if (!mesh().mover().solidBody())
      FatalErrorInFunction << "Solver transferTestSolver does not support non-solid body mesh motion"
                           << exit(FatalError);

    mesh().move();
  }
}

void
Foam::solvers::transferTestSolver::thermophysicalPredictor()
{
  volScalarField & e = thermo_.he();
  const volScalarField & Cv = thermo_.Cv();

  dimensioned<Foam::scalar> t(
      "t", T_.dimensions() / (dimLength * dimLength), mesh().time().userTimeValue());
  auto & coords = mesh().C();
  e = Cv * (dimensionedScalar(T.dimensions(), 0.01) +
            (coords.component(0) * coords.component(1) + coords.component(1) * coords.component(2) +
             coords.component(2) * coords.component(0)) *
                t);

  thermo_.correct();
}

void
Foam::solvers::transferTestSolver::solve()
{
  while (pimple_.loop())
  {
    moveMeshIfNeeded();
    thermophysicalPredictor();
  }
}
