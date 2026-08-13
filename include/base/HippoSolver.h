#pragma once

#include <Time.H>
#include <fvMesh.H>
#include <pimpleControl.H>
#include <scalar.H>

#include <string>

namespace Hippo
{

/**
 * Abstract base class for solver implementations coupled to hippo.
 *
 * This class provides the interface that the Foundation OpenFOAM version
 * obtained from `Foam::solver`. In ESI OpenFOAM, there is no unified
 * `Foam::solver` abstraction — solvers are standalone applications. This
 * class must be subclassed by each application to implement the specific
 * PIMPLE-based field equations for their solver (e.g. rhoPimpleFoam,
 * pimpleFoam, etc.).
 *
 * The subclass constructor is expected to:
 *   - Create and initialise all fields (U, p, T, rho, ...)
 *   - Create a `pimpleControl` stored as `pimple`
 *   - Optionally override `maxDeltaT()` to provide a Courant-limited step
 *
 * The following pure virtual methods must be implemented:
 *   - `preSolve()`:  set up BCs / read modified dicts before the time loop
 *   - `moveMesh()`:  update mesh position (no-op for static meshes)
 *   - `solve()`:     run the inner PIMPLE corrector loop for one time step
 *   - `postSolve()`: post-processing / convergence checks after the step
 */
class HippoSolver
{
public:
  explicit HippoSolver(Foam::fvMesh & mesh)
    : _mesh(mesh), _run_time(const_cast<Foam::Time &>(mesh.time()))
  {
  }

  virtual ~HippoSolver() = default;

  // --- Interface methods ---

  /// Called before each outer time-loop iteration to update BCs / dicts.
  virtual void preSolve() = 0;

  /// Move the mesh (override for dynamic meshes; default is a no-op).
  virtual void moveMesh() {}

  /// Execute the inner PIMPLE corrector loop for one time step.
  virtual void solve() = 0;

  /// Post-processing / checks after the solve step.
  virtual void postSolve() {}

  /// Return the maximum allowable time step (Courant constraint etc.).
  /// The default returns VGREAT (no constraint).
  virtual Foam::scalar maxDeltaT() const { return Foam::VGREAT; }

  /// True if this solver supports variable (adjustable) time stepping.
  virtual bool transient() const { return true; }

  // --- Accessors ---

  Foam::fvMesh & mesh() { return _mesh; }
  const Foam::fvMesh & mesh() const { return _mesh; }

  Foam::Time & runTime() { return _run_time; }
  const Foam::Time & runTime() const { return _run_time; }

protected:
  Foam::fvMesh & _mesh;
  Foam::Time & _run_time;
};

} // namespace Hippo
