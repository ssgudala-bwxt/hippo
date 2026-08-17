#include "HippoSolverRegistry.h"

#include <stdexcept>
#include <sstream>
#include <unordered_map>
#include <mutex>
#include <cstdlib>
#include <filesystem>
#include <dlfcn.h>

namespace Hippo
{
namespace HippoSolverRegistry
{
namespace
{
using FactoryMap = std::unordered_map<std::string, Factory>;
std::mutex & registryMutex() { static std::mutex m; return m; }
FactoryMap  & factories()    { static FactoryMap m; return m; }

std::vector<std::string>
libraryCandidates(const std::string & name)
{
  namespace fs = std::filesystem;
  const std::string libname = "lib" + name + ".so";
  std::vector<std::string> candidates;
  for (const char * env : {"FOAM_USER_LIBBIN", "FOAM_LIBBIN"})
    if (const char * dir = std::getenv(env))
      candidates.push_back((fs::path(dir) / libname).string());
  candidates.push_back(libname);
  return candidates;
}

bool
tryLoadAndRegister(const std::string & name)
{
  const std::string sym_name = "hippo_solver_factory_" + name;
  for (const auto & path : libraryCandidates(name))
  {
    void * handle = dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle) continue;
    auto * fn = reinterpret_cast<Factory>(dlsym(handle, sym_name.c_str()));
    if (fn)
    {
      std::lock_guard<std::mutex> lock(registryMutex());
      factories()[name] = fn;
      return true;
    }
    dlclose(handle);
  }
  return false;
}
} // namespace

void
registerFactory(const std::string & name, Factory factory)
{
  if (name.empty() || !factory)
    throw std::invalid_argument(
        "HippoSolverRegistry: solver registration requires a non-empty name and a factory");
  std::lock_guard<std::mutex> lock(registryMutex());
  factories()[name] = factory;
}

std::unique_ptr<HippoSolver>
create(const std::string & name, Foam::fvMesh & mesh)
{
  {
    std::lock_guard<std::mutex> lock(registryMutex());
    auto it = factories().find(name);
    if (it != factories().end())
      return std::unique_ptr<HippoSolver>(it->second(mesh));
  }

  if (!tryLoadAndRegister(name))
  {
    std::ostringstream oss;
    oss << "HippoSolverRegistry: solver '" << name << "' is not registered "
        << "and 'lib" << name << ".so' could not be found or does not export "
        << "'hippo_solver_factory_" << name << "'";
    {
      std::lock_guard<std::mutex> lock(registryMutex());
      const auto & f = factories();
      if (!f.empty())
      {
        oss << ". Registered solvers: ";
        bool first = true;
        for (const auto & [k, _] : f) { if (!first) oss << ", "; oss << k; first = false; }
      }
    }
    throw std::runtime_error(oss.str());
  }

  std::lock_guard<std::mutex> lock(registryMutex());
  return std::unique_ptr<HippoSolver>(factories().at(name)(mesh));
}

bool
hasFactory(const std::string & name)
{
  std::lock_guard<std::mutex> lock(registryMutex());
  return factories().count(name) > 0;
}

} // namespace HippoSolverRegistry
} // namespace Hippo
