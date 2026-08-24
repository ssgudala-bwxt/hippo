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
  // Directly impose the analytic profile e = Cv*(0.01 + (xy+yz+zx)*t) each
  // timestep (matching test.py's expected T_shadow reference), then let the
  // solidThermo model back out T from e via correct(). This intentionally
  // does not solve a diffusion PDE - it only needs to drive T and (via
  // wallHeatFlux) the boundary heat flux to known analytic values for the
  // variable-shadowing/functionObject-shadowing tests.
  volScalarField & e = pThermo_->he();
  tmp<volScalarField> tCv = pThermo_->Cv();
  const volScalarField & Cv = tCv();

  dimensionedScalar t("t", T_.dimensions() / (dimLength * dimLength), mesh().time().timeOutputValue());
  const volVectorField & coords = mesh().C();

  volScalarField xyzTerm = coords.component(0) * coords.component(1)
    + coords.component(1) * coords.component(2) + coords.component(2) * coords.component(0);
  volScalarField xyzTermT = xyzTerm * t;
  dimensionedScalar base(T_.dimensions(), 0.01);
  volScalarField sumTerm = base + xyzTermT;

  e = Cv * sumTerm;

  pThermo_->correct();
}

