#pragma once

#include "HippoSolver.h"

#include <fvMesh.H>
#include <solver.H>

#include <memory>

namespace Hippo
{

/**
 * Adapts an ESI OpenFOAM `Foam::solver` as a hippo `HippoSolver`.
 *
 * In ESI OpenFOAM, solver modules (e.g. `transferTestSolver`, `fluid`) are
 * standalone classes derived from `Foam::solver` and registered in the ESI
 * runtime selection table.  They are not `HippoSolver` subclasses.
 *
 * This adapter bridges the gap: `FoamProblem::initialSetup()` creates one
 * automatically by reading the solver name from the controlDict and calling
 * `Foam::solver::New(name, mesh)`, wrapping the result in this class.
 *
 * The five `HippoSolver` virtual methods are forwarded to the corresponding
 * PIMPLE-phase methods of the underlying `Foam::solver`.
 */
class FoamSolverAdapter : public HippoSolver
{
public:
  /**
   * Construct by taking ownership of an existing `Foam::solver`.
   *
   * @param mesh         the OpenFOAM fvMesh (already constructed)
   * @param foam_solver  owning pointer to the concrete ESI solver instance
   */
  FoamSolverAdapter(Foam::fvMesh & mesh, std::unique_ptr<Foam::solver> foam_solver)
    : HippoSolver(mesh), _foam_solver(std::move(foam_solver))
  {
  }

  /**
   * Convenience factory: read the solver name from the mesh's controlDict and
   * instantiate via the ESI runtime selection table.
   *
   * @param mesh  fvMesh whose `controlDict` contains a `solver` entry
   */
  static std::unique_ptr<FoamSolverAdapter> New(Foam::fvMesh & mesh)
  {
    const Foam::word solver_name =
        mesh.time().controlDict().lookup<Foam::word>("solver");

    auto foam_solver = Foam::solver::New(solver_name, mesh);
    return std::make_unique<FoamSolverAdapter>(mesh, std::move(foam_solver));
  }

  // --- HippoSolver interface ---

  void preSolve() override
  {
    _foam_solver->preSolve();
  }

  void moveMesh() override
  {
    _foam_solver->moveMesh();
  }

  /**
   * Run the inner PIMPLE corrector loop for one time step.
   *
   * ESI's `Foam::solver` exposes per-phase hooks rather than a single
   * `solve()`.  We replicate what `foamRun` does: one PIMPLE outer-corrector
   * loop calling `prePredictor → momentumPredictor → thermophysicalPredictor
   * → pressureCorrector → postCorrector` then `motionCorrector`.
   */
  void solve() override
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

  void postSolve() override
  {
    _foam_solver->postSolve();
  }

  Foam::scalar maxDeltaT() const override
  {
    return _foam_solver->maxDeltaT();
  }

  // --- Access to the underlying ESI solver ---

  Foam::solver & foamSolver() { return *_foam_solver; }
  const Foam::solver & foamSolver() const { return *_foam_solver; }

private:
  std::unique_ptr<Foam::solver> _foam_solver;
};

} // namespace Hippo
