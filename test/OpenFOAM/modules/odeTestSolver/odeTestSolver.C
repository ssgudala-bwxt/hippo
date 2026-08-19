#include "odeTestSolver.H"
#include "fvMesh.H"
#include "fvMatrices.H"
#include "fvmDdt.H"
#include "scalar.H"

extern "C" Hippo::HippoSolver *
hippo_solver_factory_odeTestSolver(Foam::fvMesh & mesh)
{
  return new Foam::solvers::odeTestSolver(mesh);
}

bool
Foam::solvers::odeTestSolver::read()
{
  maxDeltaT_ = runTime().controlDict().getOrDefault<scalar>("maxDeltaT", 1e15);
  return true;
}

Foam::solvers::odeTestSolver::odeTestSolver(fvMesh & mesh)
  : Hippo::HippoSolver(mesh),
    maxDeltaT_(1e15),
    T_(IOobject("T", mesh.time().name(), mesh, IOobject::MUST_READ, IOobject::AUTO_WRITE), mesh),
    kappa_(IOobject("kappa", mesh.time().name(), mesh, IOobject::NO_READ, IOobject::AUTO_WRITE),
           mesh,
           dimensionedScalar(dimensionSet(0, -2, 0, 0, 0), 1.0)),
    pimple_(mesh),
    T(T_),
    kappa(kappa_)
{
  read();
}

Foam::scalar
Foam::solvers::odeTestSolver::maxDeltaT() const
{
  return maxDeltaT_;
}

void
Foam::solvers::odeTestSolver::preSolve()
{
  read();
}

void
Foam::solvers::odeTestSolver::solve()
{
  while (pimple_.loop())
  {
    while (pimple_.correctNonOrthogonal())
    {
      dimensionedScalar C("C", dimensionSet(0, 0, -1, 1, 0),
                          1000.0 * mesh().time().value());
      fvScalarMatrix TEqn(fvm::ddt(T_) - C);
      TEqn.solve();
    }
  }
}