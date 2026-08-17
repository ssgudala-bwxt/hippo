#pragma once

#include "HippoSolver.h"

#include <memory>
#include <string>

namespace Hippo
{
namespace HippoSolverRegistry
{

/// Raw factory function pointer type.
using Factory = HippoSolver * (*)(Foam::fvMesh & mesh);

/**
 * Register a factory for a named solver.
 * Called from HippoSolverRegistry.C after dlopen + dlsym.
 */
void registerFactory(const std::string & name, Factory factory);

/**
 * Create a HippoSolver by name.
 *
 * If the name is not already registered, attempts to dlopen
 * lib<name>.so from $FOAM_USER_LIBBIN / $FOAM_LIBBIN and resolve the
 * symbol  hippo_solver_factory_<name>  (an extern "C" function returning
 * HippoSolver*).  Throws std::runtime_error if the solver cannot be found.
 */
std::unique_ptr<HippoSolver> create(const std::string & name, Foam::fvMesh & mesh);

bool hasFactory(const std::string & name);

} // namespace HippoSolverRegistry
} // namespace Hippo

