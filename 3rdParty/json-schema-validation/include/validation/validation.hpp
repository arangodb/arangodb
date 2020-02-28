#ifndef ARANGO_SCHEMA_VALIADTION_VALIDATION_HEADER
#define ARANGO_SCHEMA_VALIADTION_VALIDATION_HEADER 1


#include <memory>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <tao/json/from_file.hpp>
namespace tao::json {
template<template<typename...> class Traits>
class basic_schema;
}

namespace arangodb::validation {
[[nodiscard]] bool validate(tao::json::value const& doc,
                            tao::json::basic_schema<tao::json::traits> const& schema); // first step
[[nodiscard]] bool validate(VPackSlice const doc,
                            VPackOptions const*,
                            tao::json::basic_schema<tao::json::traits> const& schema); // final

[[nodiscard]] tao::json::value slice_to_value(VPackSlice const& doc,
                                              bool ingore_special = false,
                                              VPackOptions const* options = &VPackOptions::Defaults,
                                              VPackSlice const* = nullptr);

[[nodiscard]] tao::json::basic_schema<tao::json::traits>* new_schema(VPackSlice const& schema);

} // namespace arangodb::validation

#endif
