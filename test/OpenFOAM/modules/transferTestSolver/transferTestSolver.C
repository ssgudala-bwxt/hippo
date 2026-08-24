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
    pimple_(mesh),
    T(T_)
{
  // pThermo_ constructs its own T from the registry (same underlying object
  // as T_ above, since both look up "T"), and is registered in the mesh
  // object registry by solidThermo's own constructor so functionObjects can
  // find it.
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
  // Directly impose the analytic profile T = 0.01 + (xy+yz+zx)*t each
  // timestep (matching test.py's expected T_shadow reference). foam/0/T uses
  // fixedValue boundary conditions so wallHeatFlux's snGrad(he) at the walls
  // is computed from an actual value difference (wall value vs adjacent-cell
  // value) rather than being forced to zero, as it would be with zeroGradient
  // (see zeroGradientFvPatchField::snGrad(), which is hardcoded to return 0)
  // or ignoring our directly-assigned value (as gradientEnergy - the he-side
  // BC basicThermo substitutes for a T zeroGradient BC - does, since it
  // tracks its own internally-computed gradient() instead).
  //
  // NOTE: T_ is registered in the mesh's objectRegistry by this solver
  // (before pThermo_ is constructed), so basicThermo::lookupOrConstruct()
  // finds it via NO_READ/re-use and sets TOwner_ = false. That means
  // solidThermo::correct() (heSolidThermo::calculate()) will NOT recompute T
  // from he - it treats T as externally owned/updated (see
  // basicThermo::updateT()/TOwner_). So we must set T_ directly here rather
  // than going through he()/Cv()/Cp() and expecting correct() to invert it.
  // he() is then updated manually (he = Cp*T, since thermoType's energy is
  // sensibleEnthalpy) so wallHeatFlux (which reads thermo.alpha()/thermo.he())
  // sees a consistent boundary gradient. correct() is still called afterwards
  // to refresh rho_/alpha_ from the new T_.
  //
  // mesh().C() includes both cell centers (internal field) and face centers
  // (boundary field), so assigning T_/e from an expression built on coords
  // sets correct fixedValue boundary values automatically - no separate
  // boundary-only pass is needed.
  tmp<volScalarField> tCp = pThermo_->Cp();
  const volScalarField & Cp = tCp();

  dimensionedScalar t("t", T_.dimensions() / (dimLength * dimLength), mesh().time().timeOutputValue());
  const volVectorField & coords = mesh().C();

  volScalarField xyzTerm = coords.component(0) * coords.component(1)
    + coords.component(1) * coords.component(2) + coords.component(2) * coords.component(0);
  volScalarField xyzTermT = xyzTerm * t;
  dimensionedScalar base(T_.dimensions(), 0.01);
  volScalarField sumTerm = base + xyzTermT;

  T_ = sumTerm;

  volScalarField & e = pThermo_->he();
  e = Cp * T_;

  pThermo_->correct();
}

