#pragma once

#include "HippoSolver.h"

#include <memory>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace Hippo
{
namespace HippoSolverRegistry
{
using Factory = HippoSolver * (*)(Foam::fvMesh & mesh);

void registerFactory(const std::string & name, Factory factory);
std::unique_ptr<HippoSolver> create(const std::string & name, Foam::fvMesh & mesh);
bool hasFactory(const std::string & name);
}

inline bool
registerSolverModule(const char * name, HippoSolverRegistry::Factory factory)
{
  using RegisterFn = void (*)(const char *, HippoSolverRegistry::Factory);

#if defined(_WIN32)
  auto * fn =
      reinterpret_cast<RegisterFn>(GetProcAddress(GetModuleHandleA(nullptr), "hippoRegisterSolver"));
#else
  auto * fn = reinterpret_cast<RegisterFn>(dlsym(RTLD_DEFAULT, "hippoRegisterSolver"));
#endif

  if (!fn)
    return false;

  fn(name, factory);
  return true;
}
} // namespace Hippo

extern "C" void hippoRegisterSolver(const char * name, Hippo::HippoSolverRegistry::Factory factory);
