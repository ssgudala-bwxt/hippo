/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C) 2022-2024 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "DimensionedField.H"
#include "dimensionedScalar.H"
#include "dimensionedVector.H"
#include "fvMesh.H"
#include "postprocessorTestSolver.H"
#include "fvMeshMover.H"
#include "addToRunTimeSelectionTable.H"
#include "scalar.H"
#include "volFieldsFwd.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
namespace solvers
{
defineTypeNameAndDebug(postprocessorTestSolver, 0);
addToRunTimeSelectionTable(solver, postprocessorTestSolver, fvMesh);
}
}

// * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * //

// * * * * * * * * * * * * * Protected Member Functions  * * * * * * * * * * //

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //
// Solver based on solid.C module
Foam::solvers::postprocessorTestSolver::postprocessorTestSolver(fvMesh & mesh) : solver(mesh) {}

// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::solvers::postprocessorTestSolver::~postprocessorTestSolver() {}

// * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * * //

Foam::scalar
Foam::solvers::postprocessorTestSolver::maxDeltaT() const
{
  return vGreat;
}

void
Foam::solvers::postprocessorTestSolver::preSolve()
{
  fvModels().preUpdateMesh();

  // Update the mesh for topology change, mesh to mesh mapping
  mesh_.update();
}

void
Foam::solvers::postprocessorTestSolver::moveMesh()
{
  if (pimple.firstIter() || pimple.moveMeshOuterCorrectors())
  {
    // OF14: fvMeshMover no longer has solidBody() method
    // Just attempt mesh motion for test purposes
    mesh_.move();
  }
}

void
Foam::solvers::postprocessorTestSolver::thermophysicalPredictor()
{
  // Simplified postprocessor test - just apply models
  fvModels().correct();
}

void
Foam::solvers::postprocessorTestSolver::momentumTransportPredictor()
{
}

void
Foam::solvers::postprocessorTestSolver::thermophysicalTransportPredictor()
{
}

void
Foam::solvers::postprocessorTestSolver::momentumTransportCorrector()
{
}

void
Foam::solvers::postprocessorTestSolver::thermophysicalTransportCorrector()
{
}

// ************************************************************************* //
