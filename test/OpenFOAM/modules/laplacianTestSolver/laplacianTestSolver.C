#include "laplacianTestSolver.H"
#include "fvMesh.H"
#include "fvMatrices.H"
#include "fvmLaplacian.H"
#include "scalar.H"

extern "C" Hippo::HippoSolver *
hippo_solver_factory_laplacianTestSolver(Foam::fvMesh & mesh)
{
  return new Foam::solvers::laplacianTestSolver(mesh);
}

bool
Foam::solvers::laplacianTestSolver::read()
{
  maxDeltaT_ = runTime().controlDict().getOrDefault<scalar>("maxDeltaT", 1e15);
  return true;
}

Foam::solvers::laplacianTestSolver::laplacianTestSolver(fvMesh & mesh)
  : Hippo::HippoSolver(mesh),
    maxDeltaT_(1e15),
    T_(IOobject("T", mesh.time().name(), mesh, IOobject::MUST_READ, IOobject::AUTO_WRITE), mesh),
    kappa_(IOobject("kappa", mesh.time().name(), mesh, IOobject::NO_READ, IOobject::AUTO_WRITE),
           mesh,
           dimensionedScalar(dimensionSet(0, 0, 0, 0, 0), 1.0)),
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
  read();
}

void
Foam::solvers::laplacianTestSolver::solve()
{
  dimensionedScalar C(dimensionSet(0, -2, 0, 0, 0), 1.0);
  while (pimple_.loop())
  {
    while (pimple_.correctNonOrthogonal())
    {
      fvScalarMatrix TEqn(fvm::laplacian(kappa_, T_) + C * T_.oldTime());
      TEqn.solve();
    }
  }
}