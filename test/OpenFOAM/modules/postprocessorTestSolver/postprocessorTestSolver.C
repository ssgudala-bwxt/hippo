#include "postprocessorTestSolver.H"
#include "fvMesh.H"

extern "C" Hippo::HippoSolver *
hippo_solver_factory_postprocessorTestSolver(Foam::fvMesh & mesh)
{
  return new Foam::solvers::postprocessorTestSolver(mesh);
}

bool
Foam::solvers::postprocessorTestSolver::read()
{
  maxDeltaT_ = runTime().controlDict().getOrDefault<scalar>("maxDeltaT", 1e15);
  return true;
}

Foam::solvers::postprocessorTestSolver::postprocessorTestSolver(fvMesh & mesh)
  : Hippo::HippoSolver(mesh),
    maxDeltaT_(1e15),
    U_(IOobject("U", mesh.time().name(), mesh, IOobject::MUST_READ, IOobject::AUTO_WRITE), mesh),
    rho_(IOobject("rho", mesh.time().name(), mesh, IOobject::MUST_READ, IOobject::AUTO_WRITE), mesh),
    T_(IOobject("T", mesh.time().name(), mesh, IOobject::MUST_READ, IOobject::AUTO_WRITE), mesh),
    pThermo_(fluidThermo::New(mesh)),
    pimple_(mesh)
{
  read();
}

Foam::scalar
Foam::solvers::postprocessorTestSolver::maxDeltaT() const
{
  return maxDeltaT_;
}

void
Foam::solvers::postprocessorTestSolver::preSolve()
{
  read();
}

void
Foam::solvers::postprocessorTestSolver::solve()
{
  // This test solver only exists to hold U/rho fields for FoamBC/FoamPostprocessor
  // coupling exercises (e.g. FoamMassFlowRateInletBC, FoamSideAdvectiveFluxIntegral),
  // plus a T field to exercise FoamSideAverageValue/FoamSideIntegratedValue and the
  // wallHeatFlux function object. MOOSE drives U/rho via imposed boundary conditions
  // each timestep; T is driven here directly to a known analytic profile so that
  // side_average/side_integrated_value's postprocessor values evolve with time and
  // produce a non-zero, first-order-varying wall heat flux at every boundary.
  //
  // Set T = t * x (t = current time, x = cell/face x-coordinate). This matches the
  // hippo-foundation reference implementation of this test solver
  // (thermophysicalPredictor(): h = Cp * t, where t = time * x-coordinate).
  //
  // As in transferTestSolver, T_ is registered in the mesh's objectRegistry by this
  // solver before pThermo_ is constructed, so pThermo_ reuses (not owns) T_, meaning
  // correct() will not recompute T from he. We must therefore set T_ directly and
  // keep he consistent with it via basicThermo::he(p, T).
  dimensionedScalar t("t", T_.dimensions() / dimLength, mesh().time().timeOutputValue());
  const volVectorField & coords = mesh().C();

  T_ == t * coords.component(0);

  volScalarField & e = pThermo_->he();
  e == pThermo_->he(pThermo_->p(), T_)();

  pThermo_->correct();

  while (pimple_.loop())
  {
  }
}
