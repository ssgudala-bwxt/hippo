#pragma once

#include "HippoSolver.h"
#include "solidThermophysicalTransportModel.H"

namespace Foam
{
namespace solvers
{

class transferTestSolver : public Hippo::HippoSolver
{
protected:
  scalar maxDeltaT_;
  autoPtr<solidThermo> thermoPtr_;
  solidThermo & thermo_;
  volScalarField T_;
  autoPtr<solidThermophysicalTransportModel> thermophysicalTransport_;
  pimpleControl pimple_;

  bool dependenciesModified() const;
  bool read();
  void moveMeshIfNeeded();
  void thermophysicalPredictor();

public:
  const solidThermo & thermo;
  const volScalarField & T;

  explicit transferTestSolver(fvMesh & mesh);
  transferTestSolver(const transferTestSolver &) = delete;
  ~transferTestSolver() override = default;

  scalar maxDeltaT() const override;
  void preSolve() override;
  void moveMesh() override {}
  void solve() override;

  void operator=(const transferTestSolver &) = delete;
};

} // namespace solvers
} // namespace Foam
