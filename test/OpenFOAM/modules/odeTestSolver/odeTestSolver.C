#include "dimensionSet.H"
#include "dimensionSets.H"
#include "dimensionedType.H"
#include "fvcSurfaceIntegrate.H"
#include "fvConstraints.H"
#include "fvMeshMover.H"
#include "fvModels.H"
#include "fvmDdt.H"
#include "fvmLaplacian.H"
#include "localEulerDdtScheme.H"
#include "odeTestSolver.H"
#include "scalar.H"
#include "volFieldsFwd.H"

namespace
{
Hippo::HippoSolver *
createOdeTestSolver(Foam::fvMesh & mesh)
{
  return new Foam::solvers::odeTestSolver(mesh);
}

[[maybe_unused]] const bool registered_ode_test_solver =
} // namespace

bool
Foam::solvers::odeTestSolver::dependenciesModified() const
{
  return runTime().controlDict().modified();
}

bool
Foam::solvers::odeTestSolver::read()
{
  maxDeltaT_ = runTime().controlDict().found("maxDeltaT")
                   ? runTime().controlDict().lookup<scalar>("maxDeltaT", runTime().userUnits())
                   : vGreat;

  return true;
}

Foam::solvers::odeTestSolver::odeTestSolver(fvMesh & mesh)
  : Hippo::HippoSolver(mesh),
    maxDeltaT_(vGreat),
    T_(IOobject("T", mesh.time().name(), mesh, IOobject::NO_READ, IOobject::AUTO_WRITE), mesh),
    kappa_(IOobject("kappa", mesh.time().name(), mesh, IOobject::NO_READ, IOobject::AUTO_WRITE),
           mesh,
           dimensionedScalar(dimensionSet(0, -2, 0, 0, 0), 1.)),
    pimple_(mesh),
    T(T_),
    kappa(kappa_)
{
  read();
}

Foam::scalar
Foam::solvers::odeTestSolver::maxDeltaT() const
{
  return min(Foam::fvModels::New(mesh()).maxDeltaT(), maxDeltaT_);
}

void
Foam::solvers::odeTestSolver::preSolve()
{
  if (dependenciesModified())
    read();

  Foam::fvModels::New(mesh()).preUpdateMesh();
  mesh().update();
}

void
Foam::solvers::odeTestSolver::moveMeshIfNeeded()
{
  if (pimple_.firstIter() || pimple_.moveMeshOuterCorrectors())
  {
    if (!mesh().mover().solidBody())
      FatalErrorInFunction << "Solver odeTestSolver does not support non-solid body mesh motion"
                           << exit(FatalError);

    mesh().move();
  }
}

void
Foam::solvers::odeTestSolver::thermophysicalPredictor()
{
  Foam::fvModels::New(mesh()).correct();

  while (pimple_.correctNonOrthogonal())
  {
    dimensionedScalar C{dimensionSet(0, 0, -1, 1, 0), 1000 * mesh().time().userTimeValue()};
    fvScalarMatrix TEqn(Foam::fvm::ddt(T_) - C);

    auto & constraints = Foam::fvConstraints::New(mesh());
    constraints.constrain(TEqn);
    TEqn.solve();
    constraints.constrain(T_);
  }
}

void
Foam::solvers::odeTestSolver::solve()
{
  while (pimple_.loop())
  {
    moveMeshIfNeeded();
    thermophysicalPredictor();
  }
}

// ---------------------------------------------------------------------------
// Hippo factory symbol: resolved by HippoSolverRegistry via dlsym after
// dlopen("libodeTestSolver.so").  No dependency on hippo symbols required.
// ---------------------------------------------------------------------------
extern "C" Hippo::HippoSolver *
hippo_solver_factory_odeTestSolver(Foam::fvMesh & mesh)
{
  return new Foam::solvers::odeTestSolver(mesh);
}
