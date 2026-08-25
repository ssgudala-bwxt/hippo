#pragma once

#include "MooseError.h"
#include "fvCFD_moose.h"

#include <DataIO.h>
#include <type_traits>
#include <utility>

template <typename T, typename = void>
struct has_isOldTime : std::false_type
{
};

template <typename T>
struct has_isOldTime<T, std::void_t<decltype(std::declval<const T &>().isOldTime())>>
  : std::true_type
{
};

template <typename T, typename = void>
struct has_nOldTimes : std::false_type
{
};

template <typename T>
struct has_nOldTimes<T, std::void_t<decltype(std::declval<const T &>().nOldTimes())>>
  : std::true_type
{
};

template <typename T, typename = void>
struct has_oldTime : std::false_type
{
};

template <typename T>
struct has_oldTime<T, std::void_t<decltype(std::declval<T &>().oldTime(1))>> : std::true_type
{
};

template <typename T, typename = void>
struct has_oldTimeRef : std::false_type
{
};

template <typename T>
struct has_oldTimeRef<T, std::void_t<decltype(std::declval<T &>().oldTimeRef(1))>>
  : std::true_type
{
};

template <typename T, typename = void>
struct has_clearOldTimes : std::false_type
{
};

template <typename T>
struct has_clearOldTimes<T, std::void_t<decltype(std::declval<T &>().clearOldTimes())>>
  : std::true_type
{
};

// Detects GeometricField's non-const timeIndex() accessor (returns Foam::label&).
// This is the field's own bookkeeping index used e.g. by GeometricField::storeOldTimes()
// to decide whether to auto-shift its old-time chain, and separately (for CrankNicolson's
// "ddt0(...)" auxiliary fields specifically) by CrankNicolsonDdtScheme::evaluate() to decide
// whether ddt0 needs to be recomputed for the current Foam::Time::timeIndex(). See
// removeOldTime() and isDDt0Name() below for why this is only round-tripped for ddt0 fields.
template <typename T, typename = void>
struct has_timeIndex : std::false_type
{
};

template <typename T>
struct has_timeIndex<T, std::void_t<decltype(std::declval<T &>().timeIndex())>> : std::true_type
{
};

template <typename T, typename = void>
struct has_primitiveField : std::false_type
{
};

template <typename T>
struct has_primitiveField<T, std::void_t<decltype(std::declval<const T &>().primitiveField())>>
  : std::true_type
{
};

template <typename T, typename = void>
struct has_primitiveFieldRef : std::false_type
{
};

template <typename T>
struct has_primitiveFieldRef<T, std::void_t<decltype(std::declval<T &>().primitiveFieldRef())>>
  : std::true_type
{
};

template <typename T>
inline const auto &
internalFieldConst(const T & field)
{
  if constexpr (has_primitiveField<T>::value)
    return field.primitiveField();
  else
    return field.field();
}

template <typename T>
inline auto &
internalFieldRef(T & field)
{
  if constexpr (has_primitiveFieldRef<T>::value)
    return field.primitiveFieldRef();
  else
    return field.field();
}

// This function extracts the keys associated with fields of type T from the
// mesh object registry. Note for some fields, the field.name() and the
// key are not the same. *strict* indicates whether types derived from T are
// collected
// Returns true if the given registry key corresponds to an old-time field
// (i.e. one lazily created by GeometricField::oldTime()/oldTimeRef() with a
// name of the form "<baseName>_0", "<baseName>_00", etc.). Note: OpenFOAM
// ESI's GeometricField does not expose an isOldTime() member function, so
// this cannot be reliably detected via a member check (has_isOldTime is
// effectively always false for GeometricField in ESI OpenFOAM) and must
// instead rely on the naming convention OpenFOAM itself uses internally
// (see e.g. IOobjectList::prune_0()) of appending "_0" for each level of
// old time.
inline bool
isOldTimeName(const Foam::string & key)
{
  auto pos = key.rfind('_');
  if (pos == Foam::string::npos)
    return false;
  const auto suffix = key.substr(pos + 1);
  return !suffix.empty() && suffix.find_first_not_of('0') == Foam::string::npos;
}

// Returns true if the given registry key is one of CrankNicolsonDdtScheme's
// auxiliary "ddt0(...)" fields (e.g. "ddt0(T)", "ddt0(rho,U)"). These fields
// store the previous timestep's time derivative and, unlike ordinary solved
// fields, use their own timeIndex() purely as a "has this been advanced for
// the current Foam::Time::timeIndex() yet" flag (CrankNicolsonDdtScheme::
// evaluate()); they never build their own oldTime() chain, so restoring
// timeIndex() for them (see dataStoreField/dataLoadField) cannot interfere
// with GeometricField::storeOldTimes()'s unrelated use of the same member.
inline bool
isDDt0Name(const Foam::string & key)
{
  return key.starts_with("ddt0(");
}

template <typename T, bool strict>
inline std::vector<Foam::string>
getFieldkeys(const Foam::fvMesh & mesh)
{
  std::vector<Foam::string> fieldKeyList;
  for (const auto & key : mesh.template names<T>())
  {
    auto & field = mesh.lookupObjectRef<T>(key);
    bool include = !isOldTimeName(key);
    if constexpr (has_isOldTime<T>::value)
      include = include && !field.isOldTime();
    if constexpr (strict)
      include = include && Foam::isType<T>(field);
    else
      include = include && Foam::isA<T>(field);

    if (include)
    {
      fieldKeyList.push_back(key);
    }
  }

  return fieldKeyList;
}

template <class Type, class GeomMesh>
inline void
readBoundary([[maybe_unused]] istream & stream,
             [[maybe_unused]] Foam::DimensionedField<Type, GeomMesh> & field)
{
}

template <typename GeoField>
inline void
readBoundary(istream & stream, GeoField & field)
{
  for (auto & bField : field.boundaryFieldRef())
  {
    std::vector<typename GeoField::value_type> data(bField.size());
    loadHelper(stream, data, nullptr);
    std::copy(data.begin(), data.end(), bField.begin());
  }
}

// readField for GeometricFields and DimensionedFields
template <typename GeoField>
inline void
readField(std::istream & stream, GeoField & field)
{

  auto & internal = internalFieldRef(field);
  std::vector<typename GeoField::value_type> internal_data(internal.size());
  loadHelper(stream, internal_data, nullptr);

  for (auto i = 0lu; i < internal_data.size(); ++i)
  {
    internal[i] = internal_data[i];
  }

  readBoundary(stream, field);
}

template <>
inline void
readField(std::istream & stream, Foam::uniformDimensionedScalarField & field)
{
  Foam::scalar value;
  loadHelper(stream, value, nullptr);
  field.value() = value;
}

template <class Type, class GeomMesh>
inline void
writeBoundary([[maybe_unused]] ostream & stream,
              [[maybe_unused]] const Foam::DimensionedField<Type, GeomMesh> & field)
{
}

template <class GeoField>
inline void
writeBoundary(ostream & stream, const GeoField & field)
{
  for (auto & bField : field.boundaryField())
  {
    std::vector<typename GeoField::value_type> data(bField.size());
    std::copy(bField.begin(), bField.end(), data.begin());
    storeHelper(stream, data, nullptr);
  }
}

// writeField for GeometricFields and DimensionedFields
template <typename GeoField>
inline void
writeField(ostream & stream, const GeoField & field)
{
  const auto & internal = internalFieldConst(field);
  std::vector<typename GeoField::value_type> internal_field(internal.size());
  std::copy(internal.begin(), internal.end(), internal_field.begin());

  storeHelper(stream, internal_field, nullptr);

  writeBoundary(stream, field);
}

// writeField for UniformDimensionedFields
template <typename Type>
inline void
writeField(ostream & stream, const Foam::UniformDimensionedField<Type> & field)
{
  storeHelper(stream, field.value(), nullptr);
}

// Generic function for serialising any field and its old times
template <typename T>
inline void
dataStoreField(std::ostream & stream,
               const Foam::string & name,
               T & field,
               std::set<std::string> & field_list)
{
  Foam::label nOldTimes{0};
  if constexpr (has_nOldTimes<T>::value && has_oldTime<T>::value)
    nOldTimes = field.nOldTimes();
  storeHelper(stream, nOldTimes, nullptr);

  std::string field_name{name};
  storeHelper(stream, field_name, nullptr);
  writeField(stream, field);

  // CrankNicolsonDdtScheme's "ddt0(...)" auxiliary fields track whether they
  // have already been advanced for the current Foam::Time::timeIndex() via
  // their own timeIndex(). Restoring only the field values on a fixed-point
  // re-solve (without restoring this index) leaves it referring to the
  // now-current timeIndex(), so CrankNicolsonDdtScheme::evaluate() sees no
  // change and skips recomputing ddt0, silently reusing stale scheme state
  // across fixed-point iterations. Store it here, alongside the values, so
  // dataLoadField can put it back. See isDDt0Name()/has_timeIndex above.
  if constexpr (has_timeIndex<T>::value)
  {
    if (isDDt0Name(name))
    {
      Foam::label ddt0TimeIndex{field.timeIndex()};
      storeHelper(stream, ddt0TimeIndex, nullptr);
      // TEMPORARY DEBUG - remove once CN fixed-point behaviour is confirmed.
      mooseInfoRepeated("[CN-DDT0-STORE] name=" + name +
                         " meshTimeIndex=" + std::to_string(field.mesh().time().timeIndex()) +
                         " ddt0.timeIndex=" + std::to_string(ddt0TimeIndex) +
                         " val0=" + std::to_string(internalFieldConst(field)[0]));
    }
  }

  field_list.insert(name);
  if constexpr (has_nOldTimes<T>::value && has_oldTime<T>::value)
  {
    for (int n = 1; n <= nOldTimes; ++n)
    {
      writeField(stream, field.oldTime(n));
      field_list.insert(field.oldTime(n).name());
    }
  }
}

// Generic function for deserialising any field and its old times
template <typename T>
inline void
dataLoadField(std::istream & stream, Foam::fvMesh & foam_mesh)
{

  Foam::label nOldTimes;
  loadHelper(stream, nOldTimes, nullptr);

  std::string field_name;
  loadHelper(stream, field_name, nullptr);
  auto & field = foam_mesh.lookupObjectRef<T>(field_name);
  readField(stream, field);

  // Restore the ddt0(...) timeIndex() written by dataStoreField, in the same
  // order it was written, so CrankNicolsonDdtScheme::evaluate() recomputes
  // ddt0 on the next access after this restore rather than reusing stale
  // scheme state. See isDDt0Name()/has_timeIndex above.
  if constexpr (has_timeIndex<T>::value)
  {
    if (isDDt0Name(field_name))
    {
      Foam::label ddt0TimeIndex;
      loadHelper(stream, ddt0TimeIndex, nullptr);
      field.timeIndex() = ddt0TimeIndex;
      // TEMPORARY DEBUG - remove once CN fixed-point behaviour is confirmed.
      mooseInfoRepeated(
          "[CN-DDT0-LOAD] name=" + field_name +
          " meshTimeIndex=" + std::to_string(foam_mesh.time().timeIndex()) +
          " ddt0.timeIndex=" + std::to_string(field.timeIndex()) +
          " val0=" + std::to_string(internalFieldConst(field)[0]));
    }
  }

  if constexpr (has_oldTimeRef<T>::value)
  {
    for (int nOld = 1; nOld <= nOldTimes; ++nOld)
    {
      auto & old_field = field.oldTimeRef(nOld);
      readField(stream, old_field);
    }
  }
}

// serialises all fields of type T
template <typename T, bool strict>
inline void
storeFields(std::ostream & stream, const Foam::fvMesh & mesh, std::set<std::string> & field_list)
{
  const auto cur_fields{getFieldkeys<T, strict>(mesh)};
  auto nFields{static_cast<int>(cur_fields.size())};

  storeHelper(stream, nFields, nullptr);
  for (auto & key : cur_fields)
  {
    auto & field = mesh.lookupObjectRef<T>(key);

    dataStoreField<T>(stream, key, field, field_list);
  }
}

// These structs statically determine whether a class is a geometric type
// returns the first if type doesn't match a GeometricField and true if it
// does
template <typename>
struct is_geometric_field : std::false_type
{
};

template <typename Type, template <class> class Patch, typename Mesh>
struct is_geometric_field<Foam::GeometricField<Type, Patch, Mesh>> : std::true_type
{
};

template <typename T>
void
removeOldTime(Foam::fvMesh & mesh, T & field)
{
  // This is required for Hippo to behave the exact same as OpenFOAM when using
  // fixed-point iteration. The differences only affect the fvc::ddt calls on the first
  // time step. In OpenFOAM, on the first timestep fvc::ddt calls return 0. However,
  // on the second fixed-point they don't unless the old time base field is cleared, but
  // this results in an internal OpenFOAM error for some time schemes.
  //
  // Specifically, CrankNicolsonDdtScheme registers its own "ddt0(...)" auxiliary fields
  // in the mesh registry (see isDDt0Name()). Because these are ordinary GeometricFields
  // (e.g. volScalarField) as far as the type system is concerned, they are picked up by
  // this same loadFields()/removeOldTime() loop alongside the solved fields they derive
  // from. Calling clearOldTimes() directly on a ddt0(...) field disrupts the internal
  // state CrankNicolsonDdtScheme::evaluate()/coef_()/coef0_() expect it to hold, producing
  // an internal OpenFOAM error, so ddt0(...) fields must be skipped here.
  //
  // Schemes known to work:
  //   - Euler (implicit)
  // Schemes known not to work
  //   - Crank-Nicolson
  // Current behaviour: do not clear the old time base field for ddt0(...) fields. The
  // ddt0(...) field's own scheme state (its timeIndex()) is instead round-tripped through
  // dataStoreField/dataLoadField so that CrankNicolsonDdtScheme::evaluate() correctly
  // recomputes ddt0 on each fixed-point iteration rather than reusing stale state.
  if constexpr (has_clearOldTimes<T>::value)
  {
    if (!isDDt0Name(field.name()))
    {
      field.clearOldTimes();
    }
  }
}

template <typename T>
inline void
loadFields(std::istream & stream, Foam::fvMesh & mesh)
{
  int nFields{};
  loadHelper(stream, nFields, nullptr);
  for (int i = 0; i < nFields; ++i)
  {
    dataLoadField<T>(stream, mesh);
  }

  const auto cur_fields{getFieldkeys<T, false>(mesh)};
  for (const auto & key : cur_fields)
  {
    T & field = mesh.lookupObjectRef<T>(key);
    // Remove fields that haven't been stored. Important for subcycling to prevent the old
    // fields which haven't been stored being used on the first time step.
    if (mesh.time().timeIndex() == 0)
    {
      removeOldTime(mesh, field);
    }
  }
}

template <>
inline void
dataStore(std::ostream & stream, const Foam::Time & time, void * context)
{
  auto timeIndex = time.timeIndex();
  auto deltaT = time.deltaTValue();
  auto timeValue = time.value();

  storeHelper(stream, timeIndex, context);
  storeHelper(stream, deltaT, context);
  storeHelper(stream, timeValue, context);
}

template <>
inline void
dataLoad(std::istream & stream, Foam::Time & time, void * context)
{
  Foam::label timeIndex;
  Foam::scalar deltaT, timeValue;

  loadHelper(stream, timeIndex, context);
  loadHelper(stream, deltaT, context);
  loadHelper(stream, timeValue, context);

  time.setDeltaT(deltaT, false);
  // This ensures that the delta0 variable is internally updated before
  // the step allowing variable deltaT to be used
  time++;

  // reset time and time index
  time.setTime(time, timeIndex);
  time.setTime(timeValue, timeIndex);
}

// Print names of mesh table of contents entries stored and not stored
inline void
debug_print_field_names(const Foam::fvMesh & mesh, const std::set<std::string> & field_list)
{
  std::string dbg_msg = "Backed up fields: ";
  for (const auto & field : field_list)
  {
    dbg_msg += field + " ";
  }
  dbg_msg += "\nNot backed up keys in fvMesh: ";
  for (const auto & field : mesh.names())
  {
    if (std::find(field_list.begin(), field_list.end(), field) == field_list.end())
    {
      dbg_msg += field + " ";
    }
  }
  dbg_msg += "\n";
  mooseInfoRepeated(dbg_msg);
}

// Main function for storing data called as a result of the
// declareDataRecoverable in FoamMesh
template <>
inline void
dataStore(std::ostream & stream, Foam::fvMesh & mesh, void * context)
{
  storeHelper(stream, mesh.time(), context);

  std::set<std::string> dbg_field_list;

  storeFields<Foam::volScalarField, false>(stream, mesh, dbg_field_list);
  storeFields<Foam::volVectorField, false>(stream, mesh, dbg_field_list);
  storeFields<Foam::volTensorField, false>(stream, mesh, dbg_field_list);
  storeFields<Foam::volSymmTensorField, false>(stream, mesh, dbg_field_list);

  storeFields<Foam::surfaceScalarField, false>(stream, mesh, dbg_field_list);
  storeFields<Foam::surfaceVectorField, false>(stream, mesh, dbg_field_list);
  storeFields<Foam::surfaceTensorField, false>(stream, mesh, dbg_field_list);
  storeFields<Foam::surfaceSymmTensorField, false>(stream, mesh, dbg_field_list);

  storeFields<Foam::DimensionedField<Foam::scalar, Foam::volMesh>, true>(
      stream, mesh, dbg_field_list);
  storeFields<Foam::DimensionedField<Foam::vector, Foam::volMesh>, true>(
      stream, mesh, dbg_field_list);
  storeFields<Foam::DimensionedField<Foam::scalar, Foam::surfaceMesh>, true>(
      stream, mesh, dbg_field_list);
  storeFields<Foam::DimensionedField<Foam::vector, Foam::surfaceMesh>, true>(
      stream, mesh, dbg_field_list);

  storeFields<Foam::uniformDimensionedScalarField, true>(stream, mesh, dbg_field_list);

#ifdef DEBUG
  debug_print_field_names(mesh, dbg_field_list);
#endif
}

// Main function for loading data called as a result of the
// declareDataRecoverable in FoamMesh
template <>
inline void
dataLoad(std::istream & stream, Foam::fvMesh & mesh, void * context)
{
  loadHelper(stream, const_cast<Foam::Time &>(mesh.time()), context);

  loadFields<Foam::volScalarField>(stream, mesh);
  loadFields<Foam::volVectorField>(stream, mesh);
  loadFields<Foam::volTensorField>(stream, mesh);
  loadFields<Foam::volSymmTensorField>(stream, mesh);

  loadFields<Foam::surfaceScalarField>(stream, mesh);
  loadFields<Foam::surfaceVectorField>(stream, mesh);
  loadFields<Foam::surfaceTensorField>(stream, mesh);
  loadFields<Foam::surfaceSymmTensorField>(stream, mesh);

  loadFields<Foam::DimensionedField<Foam::scalar, Foam::volMesh>>(stream, mesh);
  loadFields<Foam::DimensionedField<Foam::vector, Foam::volMesh>>(stream, mesh);
  loadFields<Foam::DimensionedField<Foam::scalar, Foam::surfaceMesh>>(stream, mesh);
  loadFields<Foam::DimensionedField<Foam::vector, Foam::surfaceMesh>>(stream, mesh);

  loadFields<Foam::uniformDimensionedScalarField>(stream, mesh);
}
