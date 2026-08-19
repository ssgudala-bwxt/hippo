#pragma once

#include "ArgsUtil.h"
#include "fvCFD_moose.h"

#include <memory>
#include <mpi.h>

namespace Hippo
{

class FoamRuntime
{
public:
  FoamRuntime(const std::string & case_dir, MPI_Comm const & comm);
  FoamRuntime(const FoamRuntime & rt);

  Foam::Time & runTime() { return _runtime; }

private:
  std::string _case_dir; // stored to allow copy construction
  MPI_Comm _comm;        // stored to allow copy construction
  Foam::Time _runtime;

  // Process-wide singleton: argList (and thus UPstream::init) must only be
  // called once per process. ESI v2606 explicitly errors on a second call to
  // UPstream::setHostCommunicators. We hold cArgs alongside argList because
  // argList stores a raw pointer to argv — the backing storage must outlive it.
  static std::unique_ptr<cArgs> _s_cargs;
  static std::unique_ptr<Foam::argList> _s_arg_list;
};

}
