#include "FoamSideIntegratedFunctionObject.h"
#include "InputParameters.h"
#include "MooseEnum.h"

registerMooseObject("hippoApp", FoamSideIntegratedFunctionObject);

InputParameters
FoamSideIntegratedFunctionObject::validParams()
{
  InputParameters params = FoamSideIntegratedBase::validParams();

  MooseEnum function_objects("wallHeatFlux wallShearStress");
  params.addRequiredParam<MooseEnum>(
      "function_object", function_objects, "OpenFOAM function object");
  params.addClassDescription(
      "Class that integrates a function object over OpenFOAM boundary patches.");
  return params;
}

FoamSideIntegratedFunctionObject::FoamSideIntegratedFunctionObject(const InputParameters & params)
  : FoamSideIntegratedBase(params),
    _function_object(createFunctionObject(getParam<MooseEnum>("function_object")))
{
}

std::unique_ptr<Foam::functionObject>
FoamSideIntegratedFunctionObject::createFunctionObject(const std::string & fo_name)
{
  auto fo_dict = _foam_mesh->time().controlDict().lookupOrDefault(fo_name, Foam::dictionary());

  Foam::wordList patch_names(static_cast<Foam::label>(_boundary.size()));
  for (Foam::label i = 0; i < static_cast<Foam::label>(_boundary.size()); ++i)
    patch_names[i] = _boundary[i];

  fo_dict.set("patches", patch_names);
  fo_dict.set("writeToFile", false);

  // Use this postprocessor's own (unique) name for the underlying function
  // object rather than a fixed name. Each function object registers a field
  // named after itself (e.g. objName) in the mesh's objectRegistry, and
  // duplicate registrations (e.g. two 'wallHeatFlux' postprocessors in the
  // same input file) would otherwise fail with "Failed to store pointer".
  if (fo_name == "wallHeatFlux")
  {
    return std::make_unique<Foam::functionObjects::wallHeatFlux>(
        name(), _foam_mesh->time(), fo_dict);
  }
  else // wallShearStress
  {
    return std::make_unique<Foam::functionObjects::wallShearStress>(
        name(), _foam_mesh->time(), fo_dict);
  }
}

void
FoamSideIntegratedFunctionObject::compute()
{
  _function_object->execute();
  _value = integrateValue(_function_object->name());
}
