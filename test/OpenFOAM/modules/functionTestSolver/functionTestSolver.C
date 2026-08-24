#include "functionTestSolver.H"
#include "fvcDdt.H"
#include "fvMesh.H"
#include "scalar.H"

extern "C" Hippo::HippoSolver *
hippo_solver_factory_functionTestSolver(Foam::fvMesh & mesh)
{
  return new Foam::solvers::functionTestSolver(mesh);
}

bool
Foam::solvers::functionTestSolver::read()
{
  maxDeltaT_ = runTime().controlDict().getOrDefault<scalar>("maxDeltaT", 1e15);
  return true;
}

Foam::solvers::functionTestSolver::functionTestSolver(fvMesh & mesh)
  : Hippo::HippoSolver(mesh),
    maxDeltaT_(1e15),
    T_(IOobject("T", mesh.time().name(), mesh, IOobject::MUST_READ, IOobject::AUTO_WRITE), mesh),
    dTdt_(IOobject("dTdt", mesh.time().name(), mesh, IOobject::NO_READ, IOobject::AUTO_WRITE),
          mesh,
          dimensionedScalar(dimTemperature / dimTime, 0.0)),
    kappa_(IOobject("kappa", mesh.time().name(), mesh, IOobject::NO_READ, IOobject::AUTO_WRITE),
           mesh,
           dimensionedScalar(dimless, 1.0)),
    pimple_(mesh),
    T(T_),
    dTdt(dTdt_),
    kappa(kappa_)
{
  // Force creation of T_'s oldTime field ("T_0") up-front. fvc::ddt(T_) in
  // solve() would otherwise create it lazily on first use, which registers
  // it in the mesh objectRegistry asymmetrically across fixed-point
  // iterations (Hippo snapshots/restores all registered fields between
  // Picard iterations). Without this, a snapshot taken after the first
  // solve() contains "T_0" while a snapshot taken before it does not,
  // causing "failed lookup of T_0" when restoring.
  T_.oldTime();

  read();
}

Foam::scalar
Foam::solvers::functionTestSolver::maxDeltaT() const
{
  return maxDeltaT_;
}

void
Foam::solvers::functionTestSolver::preSolve()
{
  read();
}

void
Foam::solvers::functionTestSolver::solve()
{
  while (pimple_.loop())
  {
    dimensionedScalar T0("T0", dimTemperature, mesh().time().value());
    T_ = T0;
    dTdt_ = fvc::ddt(T_);
  }
}