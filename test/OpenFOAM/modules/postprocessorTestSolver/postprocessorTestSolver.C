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
    pThermo_(fluidThermo::New(mesh, word::null, "physicalProperties")),
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
  // coupling exercises (e.g. FoamMassFlowRateInletBC, FoamSideAdvectiveFluxIntegral).
  // No physics is solved; MOOSE drives U/rho via imposed boundary conditions each
  // timestep, so simply advance the PIMPLE loop counter without further computation.
  while (pimple_.loop())
  {
  }
}
