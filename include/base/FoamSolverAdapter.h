#pragma once

#include "HippoSolver.h"

#include <fvMesh.H>
#include <memory>
#include <string>

// Forward-declare Foam::solver so this header compiles without knowing
// where solver.H lives (its path varies across ESI v2606 installations).
// The full definition is included in FoamSolverAdapter.C.
namespace Foam { class solver; }

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
   */
  FoamSolverAdapter(Foam::fvMesh & mesh, std::unique_ptr<Foam::solver> foam_solver);

  // Destructor must be defined in the .C file where Foam::solver is complete.
  ~FoamSolverAdapter() override;

  /**
   * Convenience factory: read the solver name from the mesh's controlDict and
   * instantiate via the ESI runtime selection table.
   */
  static std::unique_ptr<FoamSolverAdapter> New(Foam::fvMesh & mesh);

  // --- HippoSolver interface ---
  void preSolve() override;
  void moveMesh() override;
  void solve() override;
  void postSolve() override;
  Foam::scalar maxDeltaT() const override;

  // --- Access to the underlying ESI solver ---
  Foam::solver & foamSolver() { return *_foam_solver; }
  const Foam::solver & foamSolver() const { return *_foam_solver; }

private:
  std::unique_ptr<Foam::solver> _foam_solver;
};

} // namespace Hippo
