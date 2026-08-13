
#include "FoamDiffusionFluxBC.h"
#include "FoamVariableBCBase.h"
#include "MooseError.h"

#include <IOdictionary.H>
#include <InputParameters.h>
#include <MooseTypes.h>
#include <fixedGradientFvPatchFields.H>
#include <optional>
#include <volFieldsFwd.H>

registerMooseObject("hippoApp", FoamDiffusionFluxBC);

namespace
{
std::optional<Foam::scalar>
inline readConstantDiffusivityFromPhysicalProperties(const Foam::fvMesh & mesh,
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
  }

  return std::nullopt;
}
}

InputParameters
FoamDiffusionFluxBC::validParams()
{
  auto params = FoamVariableBCBase::validParams();
  params.addParam<std::string>(
      "diffusivity", "kappa", "Diffusivity for BC, defaults to kappa, the thermal conducitivity.");
  params.addClassDescription("A FoamBC that imposes a fixed gradient boundary condition "
                             "on the OpenFOAM simulation");
  return params;
}

FoamDiffusionFluxBC::FoamDiffusionFluxBC(const InputParameters & params)
  : FoamVariableBCBase(params), _diffusivity(getParam<std::string>("diffusivity"))
{
}

void
FoamDiffusionFluxBC::imposeBoundaryCondition()
{
  auto & foam_mesh = _mesh->fvMesh();

  // Get subdomains this FoamBC acts on
  // TODO: replace with BoundaryRestriction member functions once FoamMesh is updated
  auto subdomains = _mesh->getSubdomainIDs(_boundary);
  for (auto subdomain : subdomains)
  {
    std::vector<Real> && grad_array = getMooseVariableArray(subdomain);

    // Get the gradient associated with the field
    auto & foam_gradient =
        _mesh->getGradientBCField<Foam::volScalarField>(subdomain, _foam_variable);
    assert(grad_array.size() == static_cast<size_t>(foam_gradient.size()));

    if (foam_mesh.foundObject<Foam::volScalarField>(_diffusivity))
    {
      auto & coeff = foam_mesh.boundary()[subdomain].lookupPatchField<Foam::volScalarField>(
          _diffusivity);
      assert(foam_gradient.size() == coeff.size());
      for (auto i = 0; i < foam_gradient.size(); ++i)
        foam_gradient[i] = grad_array[i] / coeff[i];
    }
    else
    {
      auto coeff = readConstantDiffusivityFromPhysicalProperties(foam_mesh, _diffusivity);
      if (!coeff.has_value())
        mooseError("Diffusivity '",
                   _diffusivity,
                   "' is neither a Foam volScalarField nor a scalar in constant/physicalProperties.");

      for (auto i = 0; i < foam_gradient.size(); ++i)
        foam_gradient[i] = grad_array[i] / coeff.value();
    }
  }
}
