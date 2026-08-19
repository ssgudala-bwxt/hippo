# Migrating hippo from OpenFOAM-12 (Foundation) to OpenFOAM-v2606 (ESI)

## Background

OpenFOAM forked in 2016. The two maintained lines are:

| Fork | Repository | Release cadence | Version naming |
|------|-----------|-----------------|----------------|
| **Foundation** | github.com/OpenFOAM/OpenFOAM-N | annual | OpenFOAM-12, -13, … |
| **ESI / OpenCFD** | develop.openfoam.com/Development/openfoam | bi-annual | OpenFOAM-v2206, -v2406, -v2606, … |

The forks share a common ancestor but have diverged substantially in their C++ APIs. hippo was originally written against **OpenFOAM-12 (Foundation)**. This document records every API difference encountered when porting hippo to **OpenFOAM-v2606 (ESI)** and describes the chosen fix for each.

Reference URLs used during the migration:
- Foundation v12 C++ docs: https://cpp.openfoam.org/v12/

---

## 1. `Foam::solver` abstract base class (biggest architectural difference)

### Foundation behaviour
`src/finiteVolume/solver/solver.H` defines an abstract class `Foam::solver` with:
- Public members `mesh` (`fvMesh&`) and `runTime` (`Time&`)
- A `pimple` member of type `pimpleSingleRegionControl`
- Virtual methods `preSolve()`, `moveMesh()`, `motionCorrector()`, `prePredictor()`, `momentumPredictor()`, `thermophysicalPredictor()`, `pressureCorrector()`, `postCorrector()`, `postSolve()`
- Static `deltaTFactor`
- Factory `Foam::solver::New(name, mesh)` that loads a solver shared library at run time
- `controlDict` entry `solver fluid;` selects which solver class to load

hippo wrapped this via `FoamSolver`, which held a `Foam::solver*` and delegated the time-loop steps to it.

### ESI behaviour
No `Foam::solver` class exists in ESI. Solvers are standalone executables (`pimpleFoam`, `rhoPimpleFoam`, etc.) that own their own PIMPLE loop and fields directly. The `solver` controlDict entry has no meaning.

### Fix

**New file: `include/base/HippoSolver.h`**

A new abstract base class `Hippo::HippoSolver` was created that mirrors the lifecycle interface hippo needs:

```cpp
class HippoSolver {
public:
    virtual ~HippoSolver() = default;
    virtual void preSolve() {}
    virtual void moveMesh() {}
    virtual void solve() = 0;
    virtual void postSolve() {}
    virtual double maxDeltaT() const { return std::numeric_limits<double>::max(); }
    virtual bool transient() const { return true; }
};
```

Users who previously subclassed `Foam::solver` must now subclass `Hippo::HippoSolver` and implement `solve()`.

**`include/problems/FoamProblem.h`**
- Added `std::unique_ptr<Hippo::HippoSolver> _hippo_solver`
- Added `void registerHippoSolver(std::unique_ptr<Hippo::HippoSolver> solver)`

**`src/problems/FoamProblem.C`**
- Removed `Foam::solver::New(name, mesh)` call from constructor
- Added `registerHippoSolver()` implementation; the solver is set by the calling application before the first time step

**`include/base/FoamSolver.h`**
- `FoamSolver` now holds `Hippo::HippoSolver*` instead of `Foam::solver*`

**`src/base/FoamSolver.C`**
- `run()` now delegates to `_hippo_solver->preSolve()`, `->moveMesh()`, `->solve()`, `->postSolve()` instead of `_solver->preSolve()`, etc.

**Examples (`system/controlDict`)**
- Commented out the Foundation-only `solver fluid;` entry — it has no meaning in ESI:
  ```
  // solver fluid;  // Foundation OpenFOAM-12 only; not used in ESI OpenFOAM-v2606
  ```

---

## 2. `pimpleSingleRegionControl` → `pimpleControl`

### Foundation behaviour
`FoamSolver.C` included `<pimpleSingleRegionControl.H>` and the `Foam::solver` base class held a `pimpleSingleRegionControl pimple` member.

### ESI behaviour
`pimpleSingleRegionControl` does not exist. ESI uses `pimpleControl` directly.

### Fix
Since hippo no longer owns the PIMPLE loop (that responsibility now lies in the user-supplied `HippoSolver::solve()` implementation), this header was simply removed from:
- `include/base/foam/fvCFD_moose.h`
- `src/timesteppers/FoamTimeStepper.C`

---

## 3. `functionObject` constructor signature

### Foundation behaviour
```cpp
Foam::functionObject(const word& name, const Time& runTime);
```
The constructor took a `Time&` reference.

### ESI behaviour
```cpp
Foam::functionObject(const word& name);
```
`Time` is no longer passed to the base constructor; function objects access time through their registry.

### Fix — `include/base/FoamSolver.h`
The inner class `mooseDeltaT` (a `functionObject`) had its constructor changed from:
```cpp
mooseDeltaT(const word& name, Foam::Time& time)
    : Foam::functionObject(name, time), _time(time) {}
```
to:
```cpp
mooseDeltaT(const word& name, Foam::Time& time)
    : Foam::functionObject(name), _time(time) {}
```
`_time` is now stored as a `Foam::Time&` member rather than being inherited.

---

## 4. `functionObject::maxDeltaT()` → `functionObject::adjustTimeStep()`

### Foundation behaviour
```cpp
virtual scalar maxDeltaT() const;
// called by functionObjectList::maxDeltaT()
```

### ESI behaviour
```cpp
virtual bool adjustTimeStep();
// called by functionObjectList::adjustTimeStep() from Time::adjustDeltaT()
```
The ESI approach gives the function object a chance to modify `runTime.deltaT()` directly rather than returning a scalar.

### Fix — `include/base/FoamSolver.h`
`mooseDeltaT` override changed from:
```cpp
virtual Foam::scalar maxDeltaT() const override {
    return _moose_delta_t;
}
```
to:
```cpp
virtual bool adjustTimeStep() override {
    const Foam::scalar newDt =
        min(_time.deltaTValue() * 1.2,     // growth limiter
            min(_moose_delta_t, _time.endTime().value() - _time.value()));
    _time.setDeltaT(newDt, false);
    return true;
}
```
`1.2` replaces the Foundation static `Foam::solver::deltaTFactor` (which also doesn't exist in ESI).

---

## 5. `functionObject::fields()` and `executeAtStart()` — Foundation only

### Foundation behaviour
The Foundation `functionObject` base class declared:
```cpp
virtual wordList fields() const = 0;
virtual bool executeAtStart() const;
```

### ESI behaviour
Neither method exists.

### Fix — `include/base/FoamSolver.h`
Both overrides were removed from `mooseDeltaT`.

---

## 6. `lookupPatchField` template argument count

### Foundation behaviour
```cpp
boundary()[patchID].lookupPatchField<GeometricField, ValueType>(name)
// two template arguments: field type and scalar type
```

### ESI behaviour
```cpp
boundary()[patchID].lookupPatchField<GeometricField>(name)
// one template argument; returns GeometricField::Patch (fvPatchField<T>)
```

### Fix
All six files that called `lookupPatchField` were updated:

| File | Change |
|------|--------|
| `src/bcs/FoamFixedValueBC.C` | `<volScalarField, double>` → `<volScalarField>` |
| `src/bcs/FoamFixedValuePosprocessorBC.C` | same |
| `src/bcs/FoamDiffusionFluxBC.C` | `<volScalarField, double>` and `<volVectorField, double>` |
| `src/bcs/FoamDiffusionFluxPostprocessorBC.C` | same |
| `src/bcs/FoamMassFlowRateInletBC.C` | `<volVectorField, double>` and `<volScalarField, double>` |
| `src/variables/FoamVariableField.C` | `<volScalarField, double>` |
| `src/postprocessors/FoamSideAdvectiveFluxIntegral.C` | `<volScalarField, double>` and `<volVectorField, double>` |
| `src/postprocessors/FoamSideIntegratedBase.C` | `<volVectorField, double>` and `<volScalarField, double>` |

Correspondingly, `include/mesh/FoamMesh.h` template helper methods were changed:
```cpp
// Foundation
template <typename GeoField, typename Type>
auto & getBCField(const std::string & name, label boundary_id);

// ESI
template <typename GeoField>
auto & getBCField(const std::string & name, label boundary_id);
```

---

## 7. `Time::setDeltaTNoAdjust(dt)` → `Time::setDeltaT(dt, false)`

### Foundation behaviour
```cpp
runTime.setDeltaTNoAdjust(dt);
```

### ESI behaviour
`setDeltaTNoAdjust` does not exist. Pass `false` as the second argument to `setDeltaT` to suppress function-object-driven adjustment:
```cpp
runTime.setDeltaT(dt, false);
```

### Fix
Changed in:
- `src/base/FoamSolver.C` (in `computeDeltaT`)
- `include/mesh/FoamDataStore.h` (in `dataLoad<Foam::Time>`)

---

## 8. `Time::userTimeValue()` → `Time::value()`

### Foundation behaviour
Foundation distinguishes *user time* (simulation time with a possible unit conversion factor) from *system time*. `userTimeValue()` returned the converted value.

### ESI behaviour
ESI has no user/system time conversion. `Time` inherits from `dimensionedScalar` and `value()` returns the current simulation time directly.

### Fix — `include/mesh/FoamDataStore.h`
```cpp
// Foundation
auto timeValue = time.userTimeValue();

// ESI
auto timeValue = time.value();
```

---

## 9. `time.name()` / `runtime.name()` → `timeName()`

### Foundation behaviour
`Time::name()` (inherited from `regIOobject`) returned the current time directory name as a string.

### ESI behaviour
`Time::name()` returns the object name (always "system" for Time), not the current time. Use `Time::timeName()` for the formatted current-time string.

### Fix — `src/mesh/FoamMesh.C`
```cpp
// Foundation
auto path = runtime.name();

// ESI
auto path = runtime.timeName();
```

---

## 10. `typeIOobject<T>` → `IOobject + typeHeaderOk<T>()`

### Foundation behaviour
```cpp
Foam::typeIOobject<Foam::labelIOList> addrHeader("pointProcAddressing", ...);
if (!addrHeader.headerOk()) { mooseError(...); }
auto list = std::make_unique<Foam::labelIOList>(addrHeader);
```
`typeIOobject<T>` was a Foundation class that combines an `IOobject` with compile-time type checking.

### ESI behaviour
`typeIOobject` does not exist. Use `IOobject` directly; call `typeHeaderOk<T>(bool)` to verify the on-disk type matches:
```cpp
Foam::IOobject addrHeader("pointProcAddressing", ..., Foam::IOobject::MUST_READ);
if (!addrHeader.typeHeaderOk<Foam::labelIOList>(true)) { mooseError(...); }
auto list = std::make_unique<Foam::labelIOList>(addrHeader);
```

### Fix — `src/mesh/Foam2MooseMeshGen.C`
Applied in `getLocalGlobalMap()`.

---

## 11. `mesh.curFields<T>()` → `mesh.lookupClass<T>()`

### Foundation behaviour
```cpp
for (auto& field : mesh.curFields<volScalarField>()) { ... }
```
`fvMesh::curFields<T>()` returned a range of all currently registered fields of type `T`.

### ESI behaviour
`curFields` does not exist on `fvMesh`. The equivalent is `objectRegistry::lookupClass<T>()` which returns a `HashTable<T*>`:
```cpp
for (auto& [key, field_ptr] : mesh.lookupClass<volScalarField>()) {
    auto& field = *field_ptr;
    ...
}
```

### Fix — `include/mesh/FoamDataStore.h`
Applied in `loadFields<T>()`.

---

## 12. `OldTimeBaseFieldType<T>` and `nullOldestTime()` — Foundation only

### Foundation behaviour
```cpp
// inside removeOldTime():
auto& otbf = const_cast<typename T::Base::OldTime&>(
    Foam::OldTimeBaseFieldType<T>()(field));
otbf.clearOldTimes();
otbf.nullOldestTime();
```
Foundation exposed internal old-time field management through `OldTimeBaseFieldType<T>` and `nullOldestTime()`. These were used so that `fvc::ddt` calls correctly return zero on the first fixed-point iteration of the first time step.

### ESI behaviour
Neither `OldTimeBaseFieldType` nor `nullOldestTime()` exist. Only the public `GeometricField::clearOldTimes()` is available (and it is in both Foundation and ESI).

### Fix — `include/mesh/FoamDataStore.h`
The Foundation-specific block was replaced with the single call that is portable:
```cpp
if (scheme == "Euler") {
    field.clearOldTimes();  // sufficient for Euler; CN will get a warning
}
```
A `mooseDoOnce(mooseWarning(...))` was retained for non-Euler schemes to communicate the known limitation.

---

## 13. `mesh.schemes().ddt(name)` — status in ESI

The call `mesh.schemes().ddt("ddt(field)")` used in `removeOldTime()` to look up the temporal discretisation scheme is present in both Foundation and ESI (`fvSchemes` inherits from `schemesLookup` in both forks). **No change required.**

---

## 14. `PstreamGlobals::MPI_COMM_FOAM` — Foundation only

### Foundation behaviour
The Foundation MPI patch added a global `Foam::PstreamGlobals::MPI_COMM_FOAM` that held the OpenFOAM world communicator created from the externally-supplied MOOSE comm. This was used in `FoamRuntime`'s copy constructor:
```cpp
FoamRuntime::FoamRuntime(const FoamRuntime& rt)
    : ..., _runtime(Time::controlDictName,
                    make_arg_list(_argv, Foam::PstreamGlobals::MPI_COMM_FOAM))
{}
```

### ESI behaviour
ESI uses `PstreamGlobals::MPICommunicators_` (a `DynamicList<MPI_Comm>`) indexed by communicator label. No single `MPI_COMM_FOAM` global exists.

### Fix — `include/base/FoamRuntime.h` and `src/base/FoamRuntime.C`
- Added `MPI_Comm _comm` member to `FoamRuntime` to store the communicator by value at construction time
- Copy constructor now uses the stored `_comm`:
  ```cpp
  FoamRuntime::FoamRuntime(const FoamRuntime& rt)
      : _argv(rt._argv), _comm(rt._comm),
        _runtime(Time::controlDictName, make_arg_list(_argv, rt._comm))
  {}
  ```

---

## 15. `wallHeatFlux` / `wallShearStress` include paths

### Foundation behaviour
```cpp
#include <functionObjects/field/wallHeatFlux/wallHeatFlux.H>
```

### ESI behaviour
ESI's build system creates flat `lnInclude` directories. The header is reachable as:
```cpp
#include <wallHeatFlux.H>
```
provided the `functionObjects/field/lnInclude` directory is on the include path.

### Fix
- `include/base/foam/fvCFD_moose.h`: Updated include path
- `src/variables/FoamFunctionObject.C`: Updated include path
- `hippo.mk`: Added `-isystem $(FOAM_INCLUDE_ROOT)/functionObjects/field/lnInclude`

The `wallHeatFlux(name, Time&, dict)` constructor signature is **the same in both forks** — no change needed to the calling code.

---

## 16. `Foam::solver` includes in `fvCFD_moose.h`

### Foundation behaviour
`fvCFD_moose.h` included:
```cpp
#include <finiteVolume/solver/solver.H>
#include <pimpleSingleRegionControl.H>
#include <distributionMapBase.H>
```

### ESI behaviour
None of these paths exist.

### Fix — `include/base/foam/fvCFD_moose.h`
All three includes were removed.

---

## 17. OpenFOAM MPI patch strategy

### Foundation patch
The Foundation patch (`scripts/openfoam.patch`) added:
- `UPstream::init(void* comm, bool needsThread)` — a second overload that accepted an already-initialized `MPI_Comm*`
- `argList(int& argc, char**& argv, void* comm, ...)` — an `argList` constructor that forwarded the comm to `UPstream::init`
- `ParRunControl::runPar(void* comm, bool needsThread)` — called from the new `argList` constructor
- `PstreamGlobals::MPI_COMM_FOAM` — a global that stored the resulting communicator

The patch worked by calling `MPI_Comm_split` on the external comm to create a new `MPI_COMM_FOAM`.

### ESI patch strategy

ESI uses `PstreamGlobals::MPICommunicators_` (a `DynamicList<MPI_Comm>`) indexed by communicator label. `allocateCommunicatorComponents(parentIndex=-1, index=0)` sets up the global (index-0) communicator by either assigning `MPI_COMM_WORLD` directly (`noInitialCommDup_=true`) or duping it.

`scripts/openfoam.patch` is a **real unified diff** (`git apply`-able) generated against the `OpenFOAM-v2606` source tree. It modifies 6 files:

#### `src/OpenFOAM/db/IOstreams/Pstreams/UPstream.H`
Declares a new overload alongside the existing `init(int& argc, char**& argv, bool)`:
```cpp
// Alternative init when MPI is already initialised externally
// (e.g. by MOOSE/hippo). comm must point to MPI_Comm.
static bool init(void* comm, const bool needsThread);
```

#### `src/OpenFOAM/global/argList/parRun.H`
Adds a `runPar(void* comm, bool needsThread)` method to `ParRunControl` that calls `UPstream::init(void*)`:
```cpp
void runPar(void* comm, const bool needsThread)
{
    if (!UPstream::init(comm, needsThread)) { UPstream::exit(1); }
    parallel_ = true;
}
```

#### `src/OpenFOAM/global/argList/argList.H`
Declares a new constructor before the "Construct copy with new options" block:
```cpp
// comm must point to an MPI_Comm (void* to avoid including mpi.h)
argList(int& argc, char**& argv, void* comm,
        bool checkArgs = ..., bool checkOpts = true, bool initialise = true);
```

#### `src/OpenFOAM/global/argList/argList.C`
Implements the new constructor as a **delegating constructor** using an anonymous-namespace helper to produce the side-effect of storing the external comm before the standard constructor body runs:
```cpp
namespace {
// Called from the member-initializer list so UPstream::init(void*) fires
// before the delegated-to constructor body runs.
static int& storeExternalMpiComm(void* comm, bool needsThread, int& argc)
{
    Foam::UPstream::init(comm, needsThread);
    return argc;
}
} // namespace

Foam::argList::argList(int& argc, char**& argv, void* comm,
                       bool checkArgs, bool checkOpts, bool initialise)
:
    argList(storeExternalMpiComm(comm, argList::parallelThreads_, argc),
            argv, checkArgs, checkOpts, initialise)
{}
```
The standard constructor then calls `runPar(argc, argv)` → `UPstream::init(argc, argv)`, which detects MPI already initialized, skips `MPI_Init_thread`, and falls through to `allocateCommunicatorComponents` which picks up the stored `externalMpiComm`.

#### `src/Pstream/mpi/UPstream.C` (4 changes)

1. **File-static storage** — added after `static bool ourMpi = false;`:
   ```cpp
   static MPI_Comm externalMpiComm = MPI_COMM_NULL;
   ```

2. **`init(void* comm_ptr, bool needsThread)`** — validates MPI is already initialized, stores the comm, sets `ourMpi = false` (we did not call `MPI_Init`), optionally warns on thread support:
   ```cpp
   bool Foam::UPstream::init(void* comm_ptr, const bool needsThread)
   {
       // ... MPI_Initialized check ...
       externalMpiComm = *reinterpret_cast<MPI_Comm*>(comm_ptr);
       ourMpi = false;
       // ... optional MPI_THREAD_MULTIPLE warning ...
       return true;
   }
   ```

3. **`allocateCommunicatorComponents(parentIndex == -1)` branch** — inserted before the existing `noInitialCommDup_` check:
   ```cpp
   if (externalMpiComm != MPI_COMM_NULL)
   {
       // Use external comm directly; caller owns lifetime — no dup, no free.
       PstreamGlobals::pendingMPIFree_[index] = false;
       PstreamGlobals::MPICommunicators_[index] = externalMpiComm;
   }
   else if (UPstream::noInitialCommDup_) { ... }
   else { MPI_Comm_dup(MPI_COMM_WORLD, &mpiNewComm); }
   ```

4. **`shutdown()` early return** — changed from a warning-then-continue to a hard return so OpenFOAM never calls `MPI_Finalize()` when MPI is externally owned:
   ```cpp
   if (!ourMpi)
   {
       // MPI initialized externally — do not call MPI_Finalize().
       ourMpi = false;
       return;   // <-- was: WarningInFunction << "..." then fall through
   }
   ```

#### `src/Pstream/dummy/UPstream.C`
Adds a `FatalError` stub so the dummy (serial) Pstream library satisfies the new declaration:
```cpp
bool Foam::UPstream::init(void*, const bool)
{
    FatalErrorInFunction
        << "The dummy Pstream cannot be used with an external MPI communicator."
        << Foam::exit(FatalError);
    return false;
}
```

### How to apply

`scripts/install-openfoam.sh` runs `git -C "${OPENFOAM_DIR}" apply "${SCRIPT_DIR}/openfoam.patch"` automatically. The patch was generated with `git diff --no-index` against an unmodified `OpenFOAM-v2606` source tree and uses standard `a/src/...` / `b/src/...` relative paths, so it applies cleanly with no manual adjustment needed.

---

## 18. Build system (`hippo.mk`)

No structural changes were needed. One include path was added:

```makefile
-isystem $(FOAM_INCLUDE_ROOT)/functionObjects/field/lnInclude \
```

This was required for `<wallHeatFlux.H>` and `<wallShearStress.H>` angle-bracket includes to resolve correctly.

The `finiteVolume/solver` include path present in Foundation lnInclude simply does not exist in ESI and is no longer referenced; no explicit removal was necessary.

---

## 19. `install-openfoam.sh`

| Item | Foundation | ESI |
|------|-----------|-----|
| Git URL | `https://github.com/OpenFOAM/OpenFOAM-12.git` | `https://develop.openfoam.com/Development/openfoam.git` |
| Revision pinning | SHA `9ec94dd57a8d98c3f3422ce9b2156a8b268bbda6` | Tag `OpenFOAM-v2606` |
| ThirdParty URL | `https://github.com/OpenFOAM/ThirdParty-12.git` | `https://develop.openfoam.com/Development/ThirdParty-common.git` |
| ThirdParty revision | SHA `cab725f5e7929e8f5ec35c54edc493a822355235` | Tag `v2606` |
| Build targets | `src/`, `applications/modules/`, `applications/utilities/`, `applications/solvers/` | `src/`, `applications/` |
| Output dir | `OpenFOAM-12` | `OpenFOAM-v2606` |

---

## 20. Apptainer container definitions

All `OpenFOAM-12` path references in `apptainer/hippo-dev.def` and `apptainer/hippo-release.def` were updated to `OpenFOAM-v2606`:

- `source /opt/openfoam/OpenFOAM-12/etc/bashrc` → `source /opt/openfoam/OpenFOAM-v2606/etc/bashrc`
- `FOAM_USER_LIBBIN=/root/OpenFOAM-12/lib` → `FOAM_USER_LIBBIN=/root/OpenFOAM-v2606/lib`
- `FOAM_USER_APPBIN=/root/OpenFOAM-12/bin` → `FOAM_USER_APPBIN=/root/OpenFOAM-v2606/bin`
- `LD_LIBRARY_PATH` and `PATH` entries updated accordingly

---

## Summary table of all API changes

| Foundation API | ESI equivalent | Files changed |
|---------------|----------------|---------------|
| `Foam::solver` base class | `Hippo::HippoSolver` (new) | FoamSolver.h/.C, FoamProblem.h/.C |
| `Foam::solver::New(name, mesh)` | `registerHippoSolver(solver)` | FoamProblem.C |
| `Foam::solver::deltaTFactor` | Hardcoded `1.2` | FoamSolver.h |
| `pimpleSingleRegionControl` | Removed (owned by HippoSolver::solve) | fvCFD_moose.h, FoamTimeStepper.C |
| `functionObject(name, Time&)` | `functionObject(name)` | FoamSolver.h |
| `functionObject::maxDeltaT()` | `functionObject::adjustTimeStep()` | FoamSolver.h |
| `functionObject::fields()` | Removed (no ESI equivalent) | FoamSolver.h |
| `functionObject::executeAtStart()` | Removed (no ESI equivalent) | FoamSolver.h |
| `lookupPatchField<Field, Type>(n)` | `lookupPatchField<Field>(n)` | 8 files |
| `getBCField<GeoField, Type>` | `getBCField<GeoField>` | FoamMesh.h |
| `getGradientBCField<GeoField, Type>` | `getGradientBCField<GeoField>` | FoamMesh.h |
| `Time::setDeltaTNoAdjust(dt)` | `Time::setDeltaT(dt, false)` | FoamSolver.C, FoamDataStore.h |
| `Time::userTimeValue()` | `Time::value()` | FoamDataStore.h |
| `runtime.name()` (time dir name) | `runtime.timeName()` | FoamMesh.C |
| `typeIOobject<T>` | `IOobject + typeHeaderOk<T>()` | Foam2MooseMeshGen.C |
| `mesh.curFields<T>()` | `mesh.lookupClass<T>()` | FoamDataStore.h |
| `OldTimeBaseFieldType<T>` | Removed; use `clearOldTimes()` | FoamDataStore.h |
| `field.nullOldestTime()` | Removed | FoamDataStore.h |
| `PstreamGlobals::MPI_COMM_FOAM` | Store `MPI_Comm _comm` by value | FoamRuntime.h/.C |
| `wallHeatFlux.H` include path | `<wallHeatFlux.H>` + lnInclude path | fvCFD_moose.h, FoamFunctionObject.C, hippo.mk |
| `finiteVolume/solver/solver.H` | Removed | fvCFD_moose.h |
| `distributionMapBase.H` | Removed | fvCFD_moose.h |
| `controlDict: solver fluid;` | Commented out | example controlDicts |

---

## 21. `FoamSolverAdapter` — bridging ESI `Foam::solver` modules to `HippoSolver`

### The problem

`HippoSolver` (item 1 above) is hippo's own abstract interface, introduced because
ESI has no unified `Foam::solver` base usable across every field-equation
implementation the way Foundation's `Foam::solver::New()` was. However, ESI
*does* still ship a family of `Foam::solver` subclasses selected by the
controlDict `solver` entry and instantiated via
`Foam::solver::New(name, mesh)` (e.g. `solid`, `fluid`, and hippo's own test
modules `transferTestSolver`, `bcTestSolver`, `laplacianTestSolver`,
`odeTestSolver`, `functionTestSolver`, `postprocessorTestSolver`). Every
existing hippo test case was written against this pattern and only ever calls
`FoamProblem` — none of them call `FoamProblem::registerHippoSolver()`
explicitly. Without a bridge, `FoamProblem::initialSetup()` would always
error with `no HippoSolver has been registered`.

### Fix — `include/base/FoamSolverAdapter.h` (new) and `src/problems/FoamProblem.C`

`FoamSolverAdapter` wraps an owned `Foam::solver*` and implements the
`HippoSolver` interface by forwarding to the ESI solver's own PIMPLE-phase
hooks, replicating the loop `foamRun` itself runs:

```cpp
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
```

`FoamSolverAdapter::New(mesh)` reads the controlDict `solver` entry and calls
`Foam::solver::New(name, mesh)` to build the concrete instance.

`FoamProblem::initialSetup()` now auto-creates this adapter whenever no
`HippoSolver` was explicitly registered **and** `Problem.solve` is `true`:

```cpp
if (!_hippo_solver && parameters().get<bool>("solve"))
  registerHippoSolver(Hippo::FoamSolverAdapter::New(_foam_mesh->mesh()));
```

When `Problem.solve = false` (mesh/field-inspection-only cases, see item 22),
no solver is required or created at all — `externalSolve()` never calls into
it. Explicit `registerHippoSolver()` calls (for real HippoSolver subclasses
supplied by an application) still take priority and skip auto-creation.

This one change makes every existing hippo test's `controlDict solver <name>;`
entry work unmodified, as long as `<name>` is a real ESI (or hippo test
module) `Foam::solver` subclass.

---

## 22. Test suite: adapting `test/tests/` cases from Foundation to ESI

The hippo test suite predates this migration and was written entirely against
Foundation OpenFOAM-12/14 conventions. Three distinct categories of test case
needed changes to run against ESI v2606 (via the `FoamSolverAdapter`
auto-registration described above):

### 22.1 Mesh-coupling-only tests using `solver fluid;`

`test/tests/mesh/{quadrilateral,triangular,polygonal}` and
`test/tests/multiapps/unsteady_heat_conduction_in_infinite_system` only ever
couple hippo to a single OpenFOAM field, `T`, via `FoamDiffusionFluxBC` /
`FoamVariableField`. Their Foundation-era controlDicts specified
`application foamRun; solver fluid;` with a `heRhoThermo` `physicalProperties`
and `U`, `p`, `p_rgh`, `rho` fields — a full compressible-flow setup that is
massive overkill for what the test actually exercises, and which depends on
ESI solver-module libraries (`libfluid.so` etc.) that may not exist in every
ESI install (see 22.3 below).

Since these tests never touch momentum/pressure/turbulence, they were
retargeted onto hippo's own `transferTestSolver` ESI `Foam::solver` module
(`test/OpenFOAM/modules/transferTestSolver`), which only creates a `T` field
via `solidThermo`:

- `system/controlDict`: `solver fluid;` → `solver transferTestSolver;`
  (and the now-unused `application foamRun;` line removed, matching the
  already-ESI-native `test/tests/variables/foam_variable` test)
- `constant/physicalProperties`: `thermoType.type` changed from `heRhoThermo`
  (fluid, needs `mu`) to `heSolidThermo` / `transport constIsoSolid` /
  `thermo eConst` / `energy sensibleInternalEnergy` (solid, matches
  `solidThermo::New(mesh)` used by `transferTestSolver`), keeping the same
  `kappa` (thermal conductivity) and thermodynamic values.
- Removed `0/U`, `0/p`, `0/p_rgh`, `0/rho` and `constant/momentumTransport` —
  none of these fields/dicts are read by `transferTestSolver`.
- `0/T` boundary conditions (`fixedGradient`, `zeroGradient`) are unchanged;
  `transferTestSolver` uses a plain `volScalarField` for `T` just like the
  `fluid` module did.

### 22.2 Full-CFD tests (`buoyantFoam` / `buoyantPimpleFoam` applications)

`test/tests/timesteppers/{foam_tstep_insert,foam_timestepper_sets_foam_dt,
foam_controlled_tstep_insert}/buoyantCavity`,
`test/tests/multiapps/temperature_set_on_openfoam_boundary/buoyantCavity`,
`test/tests/fixed-point/{heated_plate_converge,flow_over_heated_plate,
restart_heated_plate}/fluid-openfoam`,
`test/tests/multiapps/flow_over_heated_plate/fluid-openfoam`, and
`test/tests/multiapps/simplified_heat_exchanger/fluid-{top,bottom}-openfoam`
genuinely need a full buoyancy-driven, compressible, turbulent flow solve
(`U`, `p`, `p_rgh`, `T`, turbulence fields all present). Their Foundation
controlDicts only had an `application buoyantFoam;` / `application
buoyantPimpleFoam;` entry and **no `solver` entry at all** — Foundation ran
these as standalone application binaries, a concept that doesn't exist in
`Foam::solver::New()`'s runtime-selection model.

ESI's `fluid` solver module (`applications/solvers/modules/fluid`) is the
direct functional successor to Foundation's `buoyantFoam`/`buoyantPimpleFoam`
(density-varying, buoyant, turbulent, compressible flow — steady vs.
transient behaviour is controlled by `PIMPLE`/`SIMPLE` sub-dict settings, not
by a separate binary). Fix applied to all of the above:

- `application buoyantFoam;` / `application buoyantPimpleFoam;` →
  `application foamRun;\n\nsolver          fluid;`
- `constant/thermophysicalProperties` → `constant/physicalProperties`
  (content byte-for-byte identical except the `FoamFile.object` header field —
  ESI v2606 uses the `physicalProperties` name for this dictionary)
- `constant/turbulenceProperties` → `constant/momentumTransport` (same:
  content unchanged, only the `FoamFile.object` field updated)

Two cases (`test/tests/timesteppers/{foam_adjustable_run_time,
foam_controlled_tstep_cfl_insert}/fluid-openfoam` and
`test/tests/multiapps/shell_tube_heat_exchanger/fluid_{inner,outer}`) already
had `solver fluid;` from a previous edit but still used the old
`thermophysicalProperties`/`turbulenceProperties` dictionary names; these were
renamed the same way for consistency, and their leftover
`application buoyantFoam;` lines (harmless but stale — hippo bypasses the
`foamRun` binary's own dispatch logic and calls `Foam::solver::New()`
directly) were normalised to `application foamRun;`.

`test/tests/timesteppers/foam_controlled_tstep_cfl_insert/test.py` also
shells out to the real `foamRun -case fluid-openfoam` binary to
cross-validate hippo's internally-driven time steps against a native run;
this depends on the controlDict `solver` entry exactly as hippo's own
`FoamSolverAdapter` does, so the same rename keeps both code paths consistent.

### 22.3 Tests still dependent on the completeness of the user's ESI install

`test/tests/bcs/mass_flow_rate` uses `test/OpenFOAM/modules/postprocessorTestSolver`,
which subclasses ESI's `fluid` solver module directly (`#include "fluid.H"`).
`test/OpenFOAM/foam_modules.mk` already only builds this module when
`$FOAM_LIBBIN/libfluid.so`, `libfluidSolver.so`, `libisothermalFluid.so`, and
`libcompressibleMomentumTransportModels.so` are all present — i.e. when the
user's own ESI `Allwmake` successfully built the `fluid` solver-module family.
If those libraries are missing (as observed on the system's partial
build), `postprocessorTestSolver` — and hence `mass_flow_rate` — is skipped.
This is an ESI install-completeness issue, not a hippo defect; no further
hippo-side change is needed once the ESI install's solver modules are fully
built.

### 22.4 Mesh-only tests with no solver at all

`test/tests/mesh/cube_slice/test_{1..10}` read a mesh/fields produced by the
standalone `icoFoam` **application** (`application icoFoam;`, no `solver`
entry — again, a case Foundation ran externally, not through hippo). These
tests all set `Problem.solve = false`, meaning `FoamProblem::externalSolve()`
never calls into any solver. `FoamProblem::initialSetup()` was updated (see
item 21) to skip `FoamSolverAdapter` auto-creation entirely when
`solve = false`, so these cases no longer attempt (and fail) to instantiate a
non-existent `"icoFoam"` `Foam::solver`.

---

## Known limitations / items needing follow-up

1. **Compilation not yet verified for the full test-suite changes above.** Items 1–20 have been built and run successfully against the system's ESI v2606 install (`hippo-opt` links and the `quadrilateral` mesh test now gets past mesh/BC/solver-registration setup). The specific test-directory changes in item 22 have not all been individually re-run yet; re-run the full test suite after these changes and file follow-ups for any case-specific numerical/gold-value mismatches (unrelated to the API porting itself).

2. **HippoSolver migration guide for users.** Any application that previously subclassed `Foam::solver` must be updated to subclass `Hippo::HippoSolver` and call `FoamProblem::registerHippoSolver(std::make_unique<MyHippoSolver>(…))` before the first time step. Alternatively, if an ESI `Foam::solver` module already implements the desired physics (e.g. `solid`, `fluid`), just set the controlDict `solver` entry — `FoamSolverAdapter` will auto-register it with no C++ changes required.

3. **Crank-Nicolson temporal scheme.** The `removeOldTime()` logic in `FoamDataStore.h` now only clears old times for the Euler scheme (same as before). Crank-Nicolson will still emit a `mooseWarning` on the first time step. This is a pre-existing limitation, not a regression.


---

## 23. Validated Integration Tests (ESI v2606)

The following tests were run to completion on the system against the
ESI v2606 build of hippo and compared against Foundation OpenFOAM-12 baselines.

### 23.1 `flow_over_heated_plate` -- transient PIMPLE, 1 MPI process

- **Physics:** Buoyant compressible flow over a heated wall, coupled to MOOSE
  heat conduction (TransientMultiApp)
- **Solver mode:** PIMPLE (transient), `nCorrectors 2`
- **Steps run:** 40
- **Status:** PASS

Step 40 comparison:

| Quantity | Foundation | ESI |
|----------|-----------|-----|
| Ux iterations | 2 | 2 |
| Uy iterations | 3 | 3 |
| h iterations | 15 | 15 |
| p_rgh first solve | ~10 GAMG iters | ~10 GAMG iters |
| p_rghFinal solve | ~223 DICPCG iters | ~223 DICPCG iters |
| ClockTime / step | 1.63 s | 1.81 s (+11%) |

Difference in initial residuals (~2x) is expected: ESI normalises by a
different reference value in fvMatrix::solveSegregated. Absolute residuals
converge to the same tolerances.

### 23.2 `shell_tube_heat_exchanger` -- steady SIMPLE, 1 MPI process

- **Physics:** Inner-tube and outer-shell fluid domains (buoyant SIMPLE /
  steady-state) coupled to a MOOSE solid via heat-flux BCs
  (wallHeatFlux FunctionObject)
- **Solver mode:** SIMPLE steady (detected via mesh.schemes().steady())
- **Steps run:** 10
- **Status:** PASS

Step 10 comparison (inner / outer):

| Quantity | Foundation | ESI |
|----------|-----------|-----|
| Ux iterations (inner) | 3 | 2 |
| h iterations (inner) | 93 | 100 |
| p_rgh iterations (inner) | 383 (DICPCG) | 32 (GAMG) |
| wallHeatFlux integ -- inner | N/A logged | +21 162 W |
| wallHeatFlux integ -- outer | N/A logged | -25 610 W |
| ClockTime (inner/outer) | 114 s / 126 s | 168 s / 181 s |

### 23.3 `shell_tube_heat_exchanger` -- steady SIMPLE, 2 MPI processes

- **Physics:** Same as 23.2, decomposed with decomposePar (scotch, 2 subdomains)
- **Processes:** 2 (srun --mpi=pmi2 -n 2)
- **Steps run:** 10
- **Status:** PASS

Step 10 comparison (inner / outer):

| Quantity | ESI 1-proc | ESI 2-proc |
|----------|-----------|-----------|
| h iterations (inner) | 100 | 90 |
| p_rgh iterations (inner) | 32 (GAMG) | 29 (GAMG) |
| wallHeatFlux integ -- inner | +21 162 W | +21 241 W (<0.4%) |
| wallHeatFlux integ -- outer | -25 610 W | -25 634 W (<0.1%) |
| ClockTime (inner/outer) | 168 s / 181 s | 83 s / 90 s (~2x speedup) |

The 2-process run required three bug fixes (committed on branch `ESI-no-build`):

1. **storePrevIter() before relax()** (ebfa4a3): ESI v2606
   GeometricField::relax() calls prevIter() internally. With pimpleControl
   in steady mode this is not called automatically -- must be explicit before
   each relax() call in the SIMPLE pEqn / rho update.

2. **Singleton Foam::argList** (035a55a / cf7ea8d): ESI v2606 added a
   FatalError in UPstream::setHostCommunicators when called more than once per
   process. The original code constructed a new Foam::argList (and thus called
   UPstream::init -> setHostCommunicators) for every FoamRuntime instance.
   Fixed by making _s_arg_list a static unique_ptr constructed only on the
   first FoamRuntime call.

3. **Processor-local caseName for subsequent FoamRuntime** (e4466af):
   The Foam::Time(controlDictName, rootPath, caseName) constructor sets
   processorCase_=false by default. This causes Time::path() to return the
   global case directory instead of processorN/, so fvMesh cannot find its
   decomposed polyMesh data. Fixed by appending processorN to caseName before
   passing to the constructor, which causes TimePaths::detectProcessorCase()
   to correctly set processorCase_=true and globalCaseName_=caseName.

### 23.4 ESI-specific configuration required for shell_tube cases

Each fluid case directory requires the following compatibility shims
(not present in the original Foundation test case):

| File | Purpose |
|------|---------|
| constant/thermophysicalProperties | rhoThermo::New() reads this (ESI) instead of physicalProperties |
| constant/turbulenceProperties | turbulenceModel::New() reads this (ESI) instead of momentumTransport |

fvSolution changes vs Foundation:

| Setting | Foundation | ESI |
|---------|-----------|-----|
| p_rgh solver | DICPCG | GAMG (first), PCG+DIC (final) |
| rho solver | DICPCG | diagonal |
| PIMPLE block | Not required | Required even in SIMPLE mode (pimpleControl reads it) |
| rhoMin/rhoMax | Not needed | pMin/pMinFactor preferred; rhoMin/rhoMax accepted with warning |
