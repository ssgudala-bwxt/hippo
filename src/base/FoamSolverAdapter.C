#include "FoamSolverAdapter.h"

// solver.H is in the ESI foamRun application directory.
// Its location varies: either applications/solvers/foamRun/ (older ESI layout)
// or applications/modules/foamRun/ (newer ESI layout, v2312+).
// hippo.mk adds both paths via -isystem so one of them will resolve.
#include <solver.H>

namespace Hippo
{

FoamSolverAdapter::FoamSolverAdapter(Foam::fvMesh & mesh,
                                     std::unique_ptr<Foam::solver> foam_solver)
  : HippoSolver(mesh), _foam_solver(std::move(foam_solver))
{
}

FoamSolverAdapter::~FoamSolverAdapter() = default;

std::unique_ptr<FoamSolverAdapter>
FoamSolverAdapter::New(Foam::fvMesh & mesh)
{
  const Foam::word solver_name = mesh.time().controlDict().lookup<Foam::word>("solver");
  auto foam_solver = Foam::solver::New(solver_name, mesh);
  return std::make_unique<FoamSolverAdapter>(mesh, std::move(foam_solver));
}

void
FoamSolverAdapter::preSolve()
{
  _foam_solver->preSolve();
}

void
FoamSolverAdapter::moveMesh()
{
  _foam_solver->moveMesh();
}

void
FoamSolverAdapter::solve()
{
  auto & pimple = _foam_solver->pimple;

  while (pimple.loop())
  {
    _foam_solver->prePredictor();
    _foam_solver->momentumPredictor();
    _foam_solver->thermophysicalPredictor();
    _foam_solver->pressureCorrector();
    _foam_solver->postCorrector();
  }

  _foam_solver->motionCorrector();
}

void
FoamSolverAdapter::postSolve()
{
  _foam_solver->postSolve();
}

Foam::scalar
FoamSolverAdapter::maxDeltaT() const
{
  return _foam_solver->maxDeltaT();
}

} // namespace Hippo
