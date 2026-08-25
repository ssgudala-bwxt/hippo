#include "odeTestSolver.H"
#include "fvMesh.H"
#include "fvMatrices.H"
#include "fvmDdt.H"
#include "scalar.H"
#include <string>

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
      // TEMPORARY DEBUG - remove once CN fixed-point behaviour is confirmed.
      {
        std::string msg = "[CN-ASSEMBLY] meshTimeIndex=" +
                           std::to_string(mesh().time().timeIndex()) +
                           " time=" + std::to_string(mesh().time().value()) +
                           " C=" + std::to_string(1000.0 * mesh().time().value()) +
                           " T.mag[0]=" + std::to_string(Foam::mag(T_.primitiveField()[0])) +
                           " T.timeIndex=" + std::to_string(T_.timeIndex());
        if (mesh().foundObject<volScalarField>("ddt0(T)"))
        {
          const auto & ddt0T = mesh().lookupObject<volScalarField>("ddt0(T)");
          msg += " ddt0(T).mag[0]=" + std::to_string(Foam::mag(ddt0T.primitiveField()[0])) +
                 " ddt0(T).timeIndex=" + std::to_string(ddt0T.timeIndex());
        }
        Info << msg << endl;
      }
      TEqn.solve();
      // TEMPORARY DEBUG - remove once CN fixed-point behaviour is confirmed.
      Info << "[CN-POSTSOLVE] meshTimeIndex=" << mesh().time().timeIndex()
           << " T.mag[0]=" << Foam::mag(T_.primitiveField()[0]) << endl;
    }
  }
}