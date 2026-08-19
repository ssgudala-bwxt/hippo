#include "FoamRuntime.h"
#include "fvCFD_moose.h"

#include <MooseError.h>
#include <filesystem>

namespace Hippo
{

// Process-wide singleton storage.
// cArgs must outlive argList because argList holds a raw char** pointer.
std::unique_ptr<cArgs> FoamRuntime::_s_cargs;
std::unique_ptr<Foam::argList> FoamRuntime::_s_arg_list;

namespace
{
const std::string
checkValidCaseDir(const std::string & case_dir)
{
  namespace fs = std::filesystem;

  fs::path path{case_dir};
  if (!fs::exists(path) || !fs::is_directory(path))
    mooseError("'", case_dir, "' is not a directory.");

  if (!fs::exists(path / "0") || !fs::exists(path / "constant") || !fs::exists(path / "system"))
    mooseError("'", case_dir, "' must have directories '0', 'constant', and 'system'.");

  return case_dir;
}
} // namespace

Foam::Time
FoamRuntime::initAndMakeTime(const std::string & case_dir, MPI_Comm const & comm)
{
  namespace fs = std::filesystem;

  const std::string checked = checkValidCaseDir(case_dir);
  fs::path p{checked};
  Foam::fileName rootPath(p.parent_path().string());
  Foam::fileName caseName(p.filename().string());

  if (!_s_arg_list)
  {
    // First FoamRuntime: build argList once to call UPstream::init and set up
    // the parallel file handler (collated/uncollated IO etc.).
    auto & cargs = *(_s_cargs = std::make_unique<cArgs>("foamRun"));
    cargs.push_arg("-case");
    cargs.push_arg(checked);
    int world_size = 1;
    MPI_Comm_size(comm, &world_size);
    if (world_size > 1)
      cargs.push_arg("-parallel");

    _s_arg_list = std::make_unique<Foam::argList>(
        cargs.get_argc(), cargs.get_argv_ptr(), (void *)&comm);

    // Use the argList-based constructor for the first Time object so that the
    // parallel file handler (which argList sets up) is properly initialised.
    // This is safe because _s_arg_list was just created for this case_dir.
    return Foam::Time(Foam::Time::controlDictName, *_s_arg_list);
  }

  // Subsequent FoamRuntime instances: argList already set up MPI, the global
  // file handler, and node communicators.  Use the rootPath/caseName
  // constructor.  For parallel runs the caseName MUST include the processorN
  // component so that TimePaths::detectProcessorCase() sets processorCase_=true
  // and globalCaseName_ correctly — otherwise Time::path() returns the global
  // case dir instead of processorN/, and fvMesh cannot find polyMesh data.
  if (Foam::UPstream::parRun())
    caseName /= "processor" + Foam::name(Foam::UPstream::myProcNo());

  return Foam::Time(Foam::Time::controlDictName, rootPath, caseName);
}

FoamRuntime::FoamRuntime(const std::string & case_dir, MPI_Comm const & comm)
  : _case_dir(case_dir), _comm(comm), _runtime(initAndMakeTime(case_dir, comm))
{
}

FoamRuntime::FoamRuntime(const FoamRuntime & rt)
  : _case_dir(rt._case_dir), _comm(rt._comm), _runtime(initAndMakeTime(rt._case_dir, rt._comm))
{
}

}
