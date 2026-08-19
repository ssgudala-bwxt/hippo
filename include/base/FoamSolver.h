#pragma once

#include "HippoSolver.h"
#include "MooseError.h"

#include <Time.H>
#include <TimeState.H>
#include <functionObject.H>
#include <scalar.H>

#include <memory>
#include <optional>

namespace Foam
{
namespace functionObjects
{
/*
  The key idea is that runTime.functionObjects().adjustTimeStep() in
  adjustDeltaT loops over the function objects to potentially restrict the step.
  - By having a functionObject that returns what MOOSE wants, OpenFOAM will use
    the MOOSE time step if it is smaller than what OpenFOAM wants.
  - As a result, if MOOSE wants to add a synchronisation step OpenFOAM will also
    use it too.
  - However, the MOOSE induced cutback can lead to a slow recovery of the
    timestep and more time steps being taken than necessary.
  - So, after a cutback we modify the delta-T factor to allow the next timestep
    to return to its value before the cutback.

  In ESI OpenFOAM, the functionObject interface uses `adjustTimeStep()` instead
  of `maxDeltaT()`. The time-step adjustment is implemented here by directly
  calling `Time::setDeltaT` with the desired value when `adjustTimeStep()` is
  invoked.
*/
class mooseDeltaT : public functionObject
{
private:
  Foam::Time & _time;
  const scalar & _dt;
  std::optional<Foam::scalar> _old_desired_dt;
  const scalar _delta_t_factor;
  bool _enabled;

public:
  TypeName("mooseDeltaT")

      mooseDeltaT(const word & name, Foam::Time & runTime, const scalar & dt)
    : functionObject(name),
      _time(runTime),
      _dt(dt),
      _old_desired_dt(),
      // ESI does not have Foam::solver::deltaTFactor; use 1.2 as a sensible default
      _delta_t_factor(1.2),
      _enabled(true)
  {
  }

  virtual bool execute() override { return true; }
  virtual bool write() override { return true; }

  void setOldDesiredDt(scalar desired_dt) { _old_desired_dt = desired_dt; }
  void enable() { _enabled = true; }
  void disable() { _enabled = false; }

  Foam::scalar calculateDeltaTFactor(const Foam::scalar time) const
  {
    if (!_old_desired_dt.has_value())
      mooseError("OldDesiredTimeStep must be set before the deltaTFactor is calculated");

    if (time != _old_desired_dt)
      return _delta_t_factor * _old_desired_dt.value() / time;
    else
      return _delta_t_factor;
  }

  /// Called by Time::adjustDeltaT() — imposes MOOSE's desired time step.
  virtual bool adjustTimeStep() override
  {
    if (!_enabled || !_old_desired_dt.has_value())
      return true;

    // Adjust deltaTFactor to undo any MOOSE-induced cutback.
    const Foam::scalar factor = calculateDeltaTFactor(_time.deltaTValue());
    Foam::scalar newDeltaT = std::min(factor * _time.deltaTValue(), _dt);
    _time.setDeltaT(newDeltaT, false);
    return true;
  }
};
}
}

namespace Hippo
{
class FoamSolver
{
public:
  explicit FoamSolver(HippoSolver * solver) : _solver(solver) {}

  // Run a timestep of the OpenFOAM solver.
  void run();
  // Return the number of faces in the given patch (boundary).
  std::size_t patchSize(int patch_id);
  // Set the solver's time step size.
  void setTimeDelta(double dt) { runTime().setDeltaT(dt, false); }
  // Set the solver to the given time.
  void setCurrentTime(double time) { runTime().setTime(time, runTime().timeIndex()); }
  // Set the time at which the solver should terminate.
  void setEndTime(double time) { runTime().setEndTime(time); }
  // Run the presolve from MOOSE objects.
  void preSolve();
  // Provide access to the hippo solver.
  HippoSolver & solver() { return *_solver; };
  // Calculate OpenFOAM's time step.
  Foam::scalar computeDeltaT();
  // Check whether OpenFOAM has variable time step.
  bool isDeltaTAdjustable() const;
  // Set whether OpenFOAM can adjust the timestep.
  void setDeltaTAdjustable(const bool adjustable);
  // Get mooseDeltaT function object.
  Foam::functionObjects::mooseDeltaT & getDeltaTFunctionObject();
  // Get the current deltaT.
  Foam::scalar getTimeDelta() const { return runTime().deltaTValue(); }
  // Creates function object that tells OpenFOAM what MOOSE's time step is.
  void appendDeltaTFunctionObject(const Foam::scalar & dt);

private:
  HippoSolver * _solver = nullptr;
  std::unique_ptr<Foam::functionObjects::mooseDeltaT> _moose_dt;

  Foam::Time & runTime() { return _solver->runTime(); }
  const Foam::Time & runTime() const { return _solver->runTime(); }
};

} // namespace Hippo
