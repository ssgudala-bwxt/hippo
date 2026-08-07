#include "FoamSolver.h"
#include "HippoSolver.h"
#include "MooseError.h"

#include <IOdictionary.H>
#include <OpenFOAM/db/functionObjects/functionObjectList/functionObjectList.H>
#include <Time.H>
#include <functional>
#include <iostream>
#include <optional>
#include <ostream>
#include <scalarField.H>
#include <unistd.h>
#include <volFieldsFwd.H>

#include <algorithm>
#include <cassert>

namespace Foam
{
namespace functionObjects
{
defineTypeNameAndDebug(mooseDeltaT, 0);

}
}

namespace Hippo
{
namespace
{
/**
 * Adapted from 'applications/solvers/foamRun/setDeltaT.C' (Foundation v12).
 * In ESI OpenFOAM there is no unified Foam::solver class; this free function
 * drives the same logic using the HippoSolver interface.
 */
void
adjustDeltaT(Foam::Time & runTime, const HippoSolver & solver)
{
  if (runTime.controlDict().lookupOrDefault("adjustTimeStep", false) && solver.transient())
  {
    const Foam::scalar deltaT =
        std::min(solver.maxDeltaT(), Foam::scalar(Foam::VGREAT));

    if (deltaT < Foam::rootVGreat)
    {
      // ESI: setDeltaT(value, adjust) — pass adjust=true so adjustDeltaT()
      // (which calls functionObjects adjustTimeStep()) is also triggered.
      runTime.setDeltaT(
          std::min(1.2 * runTime.deltaTValue(), deltaT));
      std::cout << "deltaT = " << runTime.deltaTValue() << std::endl;
    }
  }
}

void
setDeltaT(Foam::Time & runTime, const HippoSolver & solver)
{
  if (runTime.timeIndex() == 0 &&
      runTime.controlDict().lookupOrDefault("adjustTimeStep", false) && solver.transient())
  {
    const Foam::scalar deltaT = solver.maxDeltaT();

    if (deltaT < Foam::rootVGreat)
    {
      runTime.setDeltaT(std::min(runTime.deltaTValue(), deltaT));
    }
  }
}

/**
 * Returns the mooseDeltaT function object if it exists.
 */
std::optional<std::reference_wrapper<Foam::functionObjects::mooseDeltaT>>
findMooseDeltaT(Foam::Time & time)
{
  auto & fo_list = time.functionObjects();
  for (int i = 0; i < fo_list.size(); ++i)
  {
    auto * ptr = dynamic_cast<Foam::functionObjects::mooseDeltaT *>(&fo_list[i]);
    if (ptr)
      return *ptr;
  }
  return std::nullopt;
}
} // namespace

/**
 * Run one time step.
 *
 * Adapted from 'applications/solvers/foamRun/foamRun.C' (Foundation v12).
 * In ESI OpenFOAM, solvers are standalone applications with their own PIMPLE
 * loop; hippo requires users to provide a HippoSolver subclass that implements
 * the field equations via `solve()`.
 */
void
FoamSolver::run()
{
  if (_solver == nullptr)
  {
    return;
  }
  auto & time = runTime();
  auto & solver = *_solver;

  // Set the initial time-step
  setDeltaT(time, solver);

  // * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

  solver.preSolve();

  // Adjust the time-step according to the solver maxDeltaT
  adjustDeltaT(time, solver);
  ++time;

  // TODO: replace std::cout with MOOSE output or a dependency-injected stream.
  std::cout << "Time = " << time.timeName() << "\n" << std::endl;

  solver.moveMesh();
  solver.solve();
  solver.postSolve();

  time.write();

  std::cout << "ExecutionTime = " << time.elapsedCpuTime() << " s"
            << "  ClockTime = " << time.elapsedClockTime() << " s"
            << "\n"
            << std::endl;
}

std::size_t
FoamSolver::patchSize(int patch_id)
{
  if (!_solver)
  {
    return 0;
  }
  auto & mesh = _solver->mesh();
  return mesh.boundary()[patch_id].size();
}

void
FoamSolver::preSolve()
{
  _solver->preSolve();
}

Foam::scalar
FoamSolver::computeDeltaT()
{
  // Determine the time-step that OpenFOAM will use on the next time step so
  // MOOSE can predict it.
  Foam::scalar deltaT = _solver->maxDeltaT();

  if (deltaT < Foam::rootVGreat)
  {
    // ESI: setDeltaT(value, adjust=true) triggers adjustDeltaT() internally,
    // which may call functionObjects' adjustTimeStep(). We probe the result then
    // restore the original value.

    // 1. Store initial value
    Foam::scalar deltaT0 = runTime().deltaTValue();
    // 2. Apply tentative setDeltaT
    runTime().setDeltaT(std::min(1.2 * runTime().deltaTValue(), deltaT));
    // 3. Retrieve adjusted value
    deltaT = runTime().deltaTValue();
    // 4. Reset without calling adjustDeltaT()
    runTime().setDeltaT(deltaT0, false);

    return deltaT;
  }
  return runTime().deltaTValue();
}

bool
FoamSolver::isDeltaTAdjustable() const
{
  return _solver->runTime().controlDict().lookupOrDefault("adjustTimeStep", false);
}

void
FoamSolver::setDeltaTAdjustable(const bool adjustable)
{
  const_cast<Foam::IOdictionary &>(
      static_cast<const Foam::IOdictionary &>(_solver->runTime().controlDict()))
      .set("adjustTimeStep", adjustable);
}

void
FoamSolver::appendDeltaTFunctionObject(const Foam::scalar & dt)
{
  // Call setDeltaT to ensure the functionObjectList dict has been read/cleared.
  runTime().setDeltaT(getTimeDelta(), false);

  // Do not recreate function object if it exists. It seems MOOSE calls
  // computeInitialDT twice.
  if (findMooseDeltaT(runTime()).has_value())
    return;

  auto moose_dt = new Foam::functionObjects::mooseDeltaT("mooseTimeStep", runTime(), dt);
  runTime().functionObjects().append(moose_dt);
}

Foam::functionObjects::mooseDeltaT &
FoamSolver::getDeltaTFunctionObject()
{
  // Return reference to function object and error if it is not found.
  auto moose_dt = findMooseDeltaT(runTime());
  if (!moose_dt.has_value())
    mooseError("MooseDeltaT function object not found. This is a bug, contact developers.");

  return *moose_dt;
}
} // namespace Hippo
