#include "FoamDiffusionFluxPostprocessorBC.h"
#include "FoamPostprocessorBCBase.h"
#include "PstreamReduceOps.H"
#include "Registry.h"
#include <IOdictionary.H>
#include <algorithm>
#include <optional>

registerMooseObject("hippoApp", FoamDiffusionFluxPostprocessorBC);

namespace
{
std::optional<Foam::scalar>
readConstantDiffusivityFromPhysicalPropertiesPP(const Foam::fvMesh & mesh,
                                                const Foam::word & name)
{
  Foam::IOdictionary physical_props(
      Foam::IOobject("physicalProperties",
                     mesh.time().constant(),
                     mesh,
                     Foam::IOobject::MUST_READ,
                     Foam::IOobject::NO_WRITE,
                     false));

  if (physical_props.found(name))
    return physical_props.get<Foam::scalar>(name);

  if (physical_props.found("mixture"))
  {
    const auto & mixture = physical_props.subDict("mixture");
    if (mixture.found("transport"))
    {
      const auto & transport = mixture.subDict("transport");
      if (transport.found(name))
        return transport.get<Foam::scalar>(name);
    }
    if (mixture.found("thermodynamics"))
    {
      const auto & thermodynamics = mixture.subDict("thermodynamics");
      if (thermodynamics.found(name))
        return thermodynamics.get<Foam::scalar>(name);
    }
  }

  return std::nullopt;
}
}

InputParameters
FoamDiffusionFluxPostprocessorBC::validParams()
{
  auto params = FoamPostprocessorBCBase::validParams();
  params.addParam<std::string>(
      "diffusivity", "kappa", "Diffusivity for BC, defaults to kappa, the thermal conducitivity.");
  return params;
}

FoamDiffusionFluxPostprocessorBC::FoamDiffusionFluxPostprocessorBC(const InputParameters & params)
  : FoamPostprocessorBCBase(params), _diffusivity(getParam<std::string>("diffusivity"))
{
}

void
FoamDiffusionFluxPostprocessorBC::imposeBoundaryCondition()
{
  auto & foam_mesh = _mesh->fvMesh();

  // Get subdomains this FoamBC acts on
  auto subdomains = _mesh->getSubdomainIDs(_boundary);
  for (auto subdomain : subdomains)
  {
    const auto & boundary = foam_mesh.boundary()[subdomain];
    // Get underlying field from OpenFOAM boundary patch.
    auto & foam_gradient =
        _mesh->getGradientBCField<Foam::volScalarField>(subdomain, _foam_variable);

    Foam::scalar coeff_bulk = 1.0;
    if (foam_mesh.foundObject<Foam::volScalarField>(_diffusivity))
    {
      const auto & coeff =
          foam_mesh.boundary()[subdomain].lookupPatchField<Foam::volScalarField>(
              _diffusivity);

      // Calculate the bulk value of the diffusivity coefficient
      const auto area = boundary.magSf();
      const auto total_area = Foam::returnReduce(Foam::sum(area), Foam::sumOp<Foam::scalar>());
      coeff_bulk =
          Foam::returnReduce(Foam::sum(coeff * area), Foam::sumOp<Foam::scalar>()) / total_area;
    }
    else
    {
      auto coeff = readConstantDiffusivityFromPhysicalPropertiesPP(foam_mesh, _diffusivity);
      if (!coeff.has_value())
        mooseError("Diffusivity '",
                   _diffusivity,
                   "' is neither a Foam volScalarField nor a scalar in constant/physicalProperties.");
      coeff_bulk = coeff.value();
    }

    // set gradient
    std::fill(foam_gradient.begin(), foam_gradient.end(), _pp_value / coeff_bulk);
  }
}
