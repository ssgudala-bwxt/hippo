#pragma once

#include "HippoSolver.h"
#include "solidThermophysicalTransportModel.H"

namespace Foam
{
namespace solvers
{

class bcTestSolver : public Hippo::HippoSolver
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

  explicit bcTestSolver(fvMesh & mesh);
  bcTestSolver(const bcTestSolver &) = delete;
  ~bcTestSolver() override = default;

  scalar maxDeltaT() const override;
  void preSolve() override;
  void moveMesh() override {}
  void solve() override;

  void operator=(const bcTestSolver &) = delete;
};

} // namespace solvers
} // namespace Foam
