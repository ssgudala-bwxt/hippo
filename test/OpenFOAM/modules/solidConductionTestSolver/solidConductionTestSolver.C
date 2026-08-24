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
    T_(IOobject("T", mesh.time().name(), mesh, IOobject::MUST_READ, IOobject::AUTO_WRITE), mesh),
    pThermo_(solidThermo::New(mesh)),
    pimple_(mesh),
    T(T_)
{
  // pThermo_ constructs its own T from the registry (same underlying object
  // as T_ above, since both look up "T"). Since T_ is registered first,
  // basicThermo::lookupOrConstruct() re-uses it (NO_READ) and sets
  // TOwner_ = false, so solidThermo::correct() will not overwrite T_ from he
  // on its own; instead updateT() infers T from he via the solidThermo
  // model's own T(p, he) inversion each time correct() is called below,
  // exactly as chtMultiRegionFoam's solid region does after solving hEqn.
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
