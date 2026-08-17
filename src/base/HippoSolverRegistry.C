#include "HippoSolverRegistry.h"

#include <stdexcept>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <cstdlib>
#include <filesystem>

namespace Hippo
{
namespace HippoSolverRegistry
{
namespace
{
using FactoryMap = std::unordered_map<std::string, Factory>;

FactoryMap &
factories()
{
  static FactoryMap value;
  return value;
}

std::unordered_set<std::string> &
loadedLibraries()
{
  static std::unordered_set<std::string> value;
  return value;
}

std::vector<void *> &
libraryHandles()
{
  static std::vector<void *> value;
  return value;
}

std::mutex &
registryMutex()
{
  static std::mutex value;
  return value;
}

std::vector<std::string>
libraryCandidates(const std::string & name)
{
  namespace fs = std::filesystem;

  std::vector<std::string> suffixes = {
#if defined(_WIN32)
      name + ".dll"
#elif defined(__APPLE__)
      "lib" + name + ".dylib",
      "lib" + name + ".so"
#else
      "lib" + name + ".so",
      "lib" + name + ".dylib"
#endif
  };

  std::vector<std::string> candidates;
  candidates.reserve(suffixes.size() * 3);

  const char * env_vars[] = {"FOAM_USER_LIBBIN", "FOAM_LIBBIN"};
  for (auto * env_var : env_vars)
    if (const char * dir = std::getenv(env_var))
      for (const auto & suffix : suffixes)
        candidates.push_back((fs::path(dir) / suffix).string());

  for (const auto & suffix : suffixes)
    candidates.push_back(suffix);

  return candidates;
}

bool
tryLoadLibrary(const std::string & name)
{
  {
    std::lock_guard<std::mutex> lock(registryMutex());
    if (loadedLibraries().count(name))
      return true;
  }

  for (const auto & candidate : libraryCandidates(name))
  {
#if defined(_WIN32)
    auto * handle = reinterpret_cast<void *>(LoadLibraryA(candidate.c_str()));
#else
    auto * handle = dlopen(candidate.c_str(), RTLD_NOW | RTLD_GLOBAL);
#endif
    if (handle)
    {
      std::lock_guard<std::mutex> lock(registryMutex());
      loadedLibraries().insert(name);
      libraryHandles().push_back(handle);
      return true;
    }
  }

  return false;
}

std::string
availableFactoryNamesUnlocked()
{
  std::ostringstream oss;
  bool first = true;
  for (const auto & [name, _] : factories())
  {
    if (!first)
      oss << ", ";
    first = false;
    oss << name;
  }
  return oss.str();
}
} // namespace

void
registerFactory(const std::string & name, Factory factory)
{
  if (name.empty() || !factory)
    throw std::invalid_argument("HippoSolverRegistry: solver registration requires a name and factory");

  std::lock_guard<std::mutex> lock(registryMutex());
  factories()[name] = factory;
}

std::unique_ptr<HippoSolver>
create(const std::string & name, Foam::fvMesh & mesh)
{
  {
    std::lock_guard<std::mutex> lock(registryMutex());
    if (auto it = factories().find(name); it != factories().end())
      return std::unique_ptr<HippoSolver>(it->second(mesh));
  }

  tryLoadLibrary(name);

  std::lock_guard<std::mutex> lock(registryMutex());
  if (auto it = factories().find(name); it != factories().end())
    return std::unique_ptr<HippoSolver>(it->second(mesh));

  std::ostringstream oss;
  oss << "HippoSolverRegistry: solver '" << name << "' is not registered";

  const auto available = availableFactoryNamesUnlocked();
  if (!available.empty())
    oss << ". Registered solvers: " << available;

  throw std::runtime_error(oss.str());
}

bool
hasFactory(const std::string & name)
{
  std::lock_guard<std::mutex> lock(registryMutex());
  return factories().count(name);
}
} // namespace HippoSolverRegistry
} // namespace Hippo

extern "C" void
hippoRegisterSolver(const char * name, Hippo::HippoSolverRegistry::Factory factory)
{
  Hippo::HippoSolverRegistry::registerFactory(name ? name : "", factory);
}
