#pragma once

#include <memory>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <tao/json/contrib/schema.hpp>

namespace arangodb::validation {
[[nodiscard]] bool validate(tao::json::value const& doc, tao::json::schema const& schema);               // first step
[[nodiscard]] bool validate(VPackSlice const doc, VPackOptions const*, tao::json::schema const& schema); // final

[[nodiscard]] tao::json::value slice_to_value(VPackSlice const& doc,
                                              bool ingore_special = false,
                                              VPackOptions const* options = &VPackOptions::Defaults,
                                              VPackSlice const* = nullptr);
[[nodiscard]] std::unique_ptr<VPackBuilder> value_to_builder(tao::json::value const& doc);
} // namespace arangodb::validation
