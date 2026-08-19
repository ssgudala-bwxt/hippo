#include "dimensionSet.H"
#include "dimensionSets.H"
#include "dimensionedType.H"
#include "fvcSurfaceIntegrate.H"
#include "fvConstraints.H"
#include "fvmLaplacian.H"
#include "laplacianTestSolver.H"
#include "localEulerDdtScheme.H"
#include "scalar.H"
#include "volFieldsFwd.H"

namespace
{
Hippo::HippoSolver *
createLaplacianTestSolver(Foam::fvMesh & mesh)
{
  return new Foam::solvers::laplacianTestSolver(mesh);
}

[[maybe_unused]] const bool registered_laplacian_test_solver =
} // namespace

bool
Foam::solvers::laplacianTestSolver::dependenciesModified() const
{
  return runTime().controlDict().modified();
}

bool
Foam::solvers::laplacianTestSolver::read()
{
  maxDeltaT_ = runTime().controlDict().found("maxDeltaT")
                   ? runTime().controlDict().lookup<scalar>("maxDeltaT", runTime().userUnits())
                   : vGreat;

  return true;
}

Foam::solvers::laplacianTestSolver::laplacianTestSolver(fvMesh & mesh)
  : Hippo::HippoSolver(mesh),
    maxDeltaT_(vGreat),
    T_(IOobject("T", mesh.time().name(), mesh, IOobject::NO_READ, IOobject::AUTO_WRITE), mesh),
    kappa_(IOobject("kappa", mesh.time().name(), mesh, IOobject::NO_READ, IOobject::AUTO_WRITE),
           mesh,
           dimensionedScalar(dimensionSet(0, 0, 0, 0, 0), 1.)),
    pimple_(mesh),
    T(T_),
    kappa(kappa_)
{
  read();
}

Foam::scalar
Foam::solvers::laplacianTestSolver::maxDeltaT() const
{
  return maxDeltaT_;
}

void
Foam::solvers::laplacianTestSolver::preSolve()
{
  if (dependenciesModified())
    read();

    }

void
Foam::solvers::laplacianTestSolver::moveMeshIfNeeded()
{
  if (pimple_.firstIter() || pimple_.moveMeshOuterCorrectors())
  {
    if (!mesh().mover().solidBody())
      FatalErrorInFunction << "Solver laplacianTestSolver does not support non-solid body mesh motion"
                           << exit(FatalError);

    mesh().move();
  }
}

void
Foam::solvers::laplacianTestSolver::thermophysicalPredictor()
{
  Foam::fvModels::New(mesh()).correct();

  dimensionedScalar C(dimensionSet(0, -2, 0, 0, 0), 1.);
  while (pimple_.correctNonOrthogonal())
  {
    fvScalarMatrix TEqn(Foam::fvm::laplacian(T_, "T") + C * T_.oldTime());

    auto & constraints = Foam::fvConstraints::New(mesh());
    constraints.constrain(TEqn);
    TEqn.solve();
    constraints.constrain(T_);
  }
}

void
Foam::solvers::laplacianTestSolver::solve()
{
  while (pimple_.loop())
  {
    moveMeshIfNeeded();
    thermophysicalPredictor();
  }
}

// ---------------------------------------------------------------------------
// Hippo factory symbol: resolved by HippoSolverRegistry via dlsym after
// dlopen("liblaplacianTestSolver.so").  No dependency on hippo symbols required.
// ---------------------------------------------------------------------------
extern "C" Hippo::HippoSolver *
hippo_solver_factory_laplacianTestSolver(Foam::fvMesh & mesh)
{
  return new Foam::solvers::laplacianTestSolver(mesh);
}
