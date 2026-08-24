#include "transferTestSolver.H"
#include "fvMesh.H"
#include "fvMatrices.H"
#include "fvmDdt.H"
#include "scalar.H"

// ---------------------------------------------------------------------------
// Hippo factory symbol: resolved by HippoSolverRegistry via dlsym after
// dlopen("libtransferTestSolver.so").
// ---------------------------------------------------------------------------
extern "C" Hippo::HippoSolver *
hippo_solver_factory_transferTestSolver(Foam::fvMesh & mesh)
{
  return new Foam::solvers::transferTestSolver(mesh);
}

bool
Foam::solvers::transferTestSolver::dependenciesModified() const
{
  // ESI v2606 dictionary has no modified(); always re-read for simplicity.
  return true;
}

bool
Foam::solvers::transferTestSolver::read()
{
  maxDeltaT_ = runTime().controlDict().getOrDefault<scalar>("maxDeltaT", 1e15);
  return true;
}

Foam::solvers::transferTestSolver::transferTestSolver(fvMesh & mesh)
  : Hippo::HippoSolver(mesh),
    maxDeltaT_(1e15),
    T_(IOobject("T", mesh.time().name(), mesh, IOobject::MUST_READ, IOobject::AUTO_WRITE), mesh),
    pThermo_(solidThermo::New(mesh)),
    kappa_("kappa", dimViscosity, 1e-5),
    pimple_(mesh),
    T(T_)
{
  // pThermo_ is registered in the mesh object registry by solidThermo's own
  // constructor; it is only used here so that functionObjects (e.g.
  // wallHeatFlux) can find a valid solidThermo model to compute alpha()/he()
  // from. This solver itself still integrates T_ directly via a simple
  // Laplacian, so pThermo_'s internal energy field is not otherwise used.
  read();
}

Foam::scalar
Foam::solvers::transferTestSolver::maxDeltaT() const
{
  return maxDeltaT_;
}

void
Foam::solvers::transferTestSolver::preSolve()
{
  if (dependenciesModified())
    read();
}

void
Foam::solvers::transferTestSolver::solve()
{
  while (pimple_.loop())
  {
    // Transient heat diffusion: dT/dt = kappa * laplacian(T)
    // Requires both ddt and laplacian to avoid singular all-Neumann system.
    fvScalarMatrix TEqn(fvm::ddt(T_) - fvm::laplacian(kappa_, T_));
    TEqn.relax();
    TEqn.solve();
  }
}

