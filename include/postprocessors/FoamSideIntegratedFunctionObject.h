#pragma once

#include "FoamSideIntegratedBase.h"
#include "InputParameters.h"

#include <memory>

class FoamSideIntegratedFunctionObject : public FoamSideIntegratedBase
{
public:
  static InputParameters validParams();

  FoamSideIntegratedFunctionObject(const InputParameters & params);

  virtual void compute() override;

protected:
  /// Creates function objects to be executed by compute
  std::unique_ptr<Foam::functionObject> createFunctionObject(const std::string & fo_name);

  /// Name of the OpenFOAM field registered by the function object (may
  /// differ from _function_object->name() since the FO's result field is
  /// renamed to a unique name after construction; see createFunctionObject).
  /// Must be declared before _function_object: member initialization follows
  /// declaration order, and createFunctionObject() (invoked to initialize
  /// _function_object) writes to _field_name, so _field_name must already be
  /// constructed by that point.
  std::string _field_name;

  std::unique_ptr<Foam::functionObject> _function_object;
};
