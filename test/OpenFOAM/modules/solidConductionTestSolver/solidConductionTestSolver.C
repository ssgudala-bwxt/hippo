#include "solidConductionTestSolver.H"
#include "fvMesh.H"
#include "fvMatrices.H"
#include "fvmDdt.H"
#include "scalar.H"

// ---------------------------------------------------------------------------
// Hippo factory symbol: resolved by HippoSolverRegistry via dlsym after
// dlopen("libsolidConductionTestSolver.so").
// ---------------------------------------------------------------------------
extern "C" Hippo::HippoSolver *
hippo_solver_factory_solidConductionTestSolver(Foam::fvMesh & mesh)
{
  return new Foam::solvers::solidConductionTestSolver(mesh);
}

bool
Foam::solvers::solidConductionTestSolver::read()
{
  maxDeltaT_ = runTime().controlDict().getOrDefault<scalar>("maxDeltaT", 1e15);
  return true;
}

Foam::solvers::solidConductionTestSolver::solidConductionTestSolver(fvMesh & mesh)
  : Hippo::HippoSolver(mesh),
    maxDeltaT_(1e15),
    pThermo_(solidThermo::New(mesh)),
    pimple_(mesh),
    T(pThermo_->T())
{
  // Deliberately do NOT pre-register a "T" volScalarField before constructing
  // pThermo_. basicThermo::lookupOrConstruct() only sets TOwner_ = true (and
  // thus allows solidThermo::correct() to recompute T from he each call) when
  // it is the one to construct/register T itself. If T were already
  // registered (e.g. by this solver, as transferTestSolver deliberately
  // does), TOwner_ would be false and correct() would silently skip updating
  // T from he - which is wrong here, since we solve he() directly and rely
  // on correct() to derive T from it.
  read();
}

Foam::scalar
Foam::solvers::solidConductionTestSolver::maxDeltaT() const
{
  return maxDeltaT_;
}

void
Foam::solvers::solidConductionTestSolver::preSolve()
{
  read();
}

void
Foam::solvers::solidConductionTestSolver::solve()
{
  // Solves rho*Cp*dT/dt = div(kappa*grad(T)) via the enthalpy form, matching
  // applications/solvers/heatTransfer/chtMultiRegionFoam/solid/solveSolid.H:
  //   fvm::ddt(betav*rho, h) - thermo.heatDiffusion(betav, h) == fvOptions
  // with betav (porosity) set to 1 everywhere since this is a single,
  // fully-solid region.
  while (pimple_.loop())
  {
    volScalarField & he = pThermo_->he();
    tmp<volScalarField> tRho = pThermo_->rho();
    const volScalarField & rho = tRho();

    volScalarField betav(
        IOobject("betav", mesh().time().timeName(), mesh(), IOobject::NO_READ, IOobject::NO_WRITE),
        mesh(),
        dimensionedScalar(dimless, 1.0));

    fvScalarMatrix hEqn(fvm::ddt(betav * rho, he) - pThermo_->heatDiffusion(betav, he));

    hEqn.solve();

    pThermo_->correct();
  }
}
