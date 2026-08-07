#pragma once

#include "ArgsUtil.h"
#include "fvCFD_moose.h"

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
  cArgs _argv;
  MPI_Comm _comm; // stored to allow copy construction
  Foam::Time _runtime;
};

}
