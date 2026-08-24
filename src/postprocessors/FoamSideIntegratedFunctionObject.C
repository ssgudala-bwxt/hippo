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

  // wallHeatFlux/wallShearStress register their result field in the
  // objectRegistry under the fixed name scopedName(typeName), e.g.
  // "wallHeatFlux" -- this scoped name is computed once, in the FO
  // constructor's initializer list, *before* read(dict) runs, so setting
  // "useNamePrefix" in the dict has no effect at construction time.
  // Instead, temporarily flip the process-wide default so the FO computes a
  // unique "<name>:<typeName>" registry entry from the start. Without this,
  // two postprocessors using the same function_object type in one input
  // file would collide on the same fixed objectRegistry entry.
  const bool old_default_use_name_prefix = Foam::functionObject::defaultUseNamePrefix;
  Foam::functionObject::defaultUseNamePrefix = true;

  std::unique_ptr<Foam::functionObject> fo;
  if (fo_name == "wallHeatFlux")
    fo = std::make_unique<Foam::functionObjects::wallHeatFlux>(name(), _foam_mesh->time(), fo_dict);
  else // wallShearStress
    fo = std::make_unique<Foam::functionObjects::wallShearStress>(
        name(), _foam_mesh->time(), fo_dict);

  Foam::functionObject::defaultUseNamePrefix = old_default_use_name_prefix;

  _field_name = name() + ":" + fo_name;

  return fo;
}

void
FoamSideIntegratedFunctionObject::compute()
{
  _function_object->execute();
  _value = integrateValue(_field_name);
}
