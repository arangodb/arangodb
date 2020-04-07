#ifndef ARANGO_SCHEMA_VALIADTION_VALIDATION_HEADER
#define ARANGO_SCHEMA_VALIADTION_VALIDATION_HEADER 1


#include <memory>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "types.hpp"
#include <tao/json/from_file.hpp>

namespace tao::json {
template<template<typename...> class Traits>
class basic_schema;
}

namespace arangodb::validation {
[[nodiscard]] bool validate(tao::json::basic_schema<tao::json::traits> const& schema,
                            SpecialProperties special,
                            VPackSlice const doc,
                            VPackOptions const*);


[[nodiscard]] tao::json::value slice_to_value(VPackSlice const& doc,
                                              SpecialProperties special = SpecialProperties::None,
                                              VPackOptions const* options = &VPackOptions::Defaults,
                                              VPackSlice const* = nullptr);

[[nodiscard]] tao::json::basic_schema<tao::json::traits>* new_schema(VPackSlice const& schema);

} // namespace arangodb::validation

#endif
