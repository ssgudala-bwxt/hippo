#include "bcTestSolver.H"
#include "fvmDdt.H"
#include "fvMesh.H"
#include "fvMatrices.H"
#include "scalar.H"

extern "C" Hippo::HippoSolver *
hippo_solver_factory_bcTestSolver(Foam::fvMesh & mesh)
{
  return new Foam::solvers::bcTestSolver(mesh);
}

bool
Foam::solvers::bcTestSolver::read()
{
  maxDeltaT_ = runTime().controlDict().getOrDefault<scalar>("maxDeltaT", 1e15);
  return true;
}

Foam::solvers::bcTestSolver::bcTestSolver(fvMesh & mesh)
  : Hippo::HippoSolver(mesh),
    maxDeltaT_(1e15),
    T_(IOobject("T", mesh.time().name(), mesh, IOobject::MUST_READ, IOobject::AUTO_WRITE), mesh),
    kappa_("kappa", dimViscosity, 1e-5),
    pimple_(mesh),
    T(T_)
{
  read();
}

Foam::scalar
Foam::solvers::bcTestSolver::maxDeltaT() const
{
  return maxDeltaT_;
}

void
Foam::solvers::bcTestSolver::preSolve()
{
  read();
}

void
Foam::solvers::bcTestSolver::solve()
{
  while (pimple_.loop())
  {
    // Pure (steady) Laplace solve: T is re-solved to convergence against the
    // current boundary conditions every call, with no transient (ddt) term.
    // This lets each MOOSE/OpenFOAM coupling step reproduce the exact
    // analytical Laplace solution implied by the imposed BCs, matching what
    // the "laplace_*"/"*_pp" BC tests check against.
    fvScalarMatrix TEqn(fvm::laplacian(kappa_, T_));
    TEqn.relax();
    TEqn.solve();
  }
}