////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "attributes.hpp"

#include "absl/strings/str_cat.h"
#include "utils/register.hpp"

namespace {

class attribute_register
  : public irs::tagged_generic_register<std::string_view, irs::type_info,
                                        std::string_view, attribute_register> {
};

}  // namespace

namespace irs {

bool attributes::exists(std::string_view name, bool load_library /*= true*/) {
  return static_cast<bool>(
    attribute_register::instance().get(name, load_library));
}

type_info attributes::get(std::string_view name,
                          bool load_library /*= true*/) noexcept {
  try {
    return attribute_register::instance().get(name, load_library);
  } catch (...) {
    IRS_LOG_ERROR(
      "Caught exception while getting an attribute instance");  // cppcheck-suppress
                                                                // syntaxError
  }

  return {};  // invalid type id
}

attribute_registrar::attribute_registrar(const type_info& type,
                                         const char* source /*= nullptr*/) {
  const auto source_ref =
    source ? std::string_view{source} : std::string_view{};
  auto entry = attribute_register::instance().set(
    type.name(), type, IsNull(source_ref) ? nullptr : &source_ref);

  registered_ = entry.second;

  if (!registered_ && type != entry.first) {
    auto* registered_source = attribute_register::instance().tag(type.name());

    if (source && registered_source) {
      IRS_LOG_WARN(
        absl::StrCat("type name collision detected while registering "
                     "attribute, ignoring: type '",
                     type.name(), "' from ", source_ref, ", previously from ",
                     *registered_source));
    } else if (source) {
      IRS_LOG_WARN(
        absl::StrCat("type name collision detected while registering "
                     "attribute, ignoring: type '",
                     type.name(), "' from ", source_ref));
    } else if (registered_source) {
      IRS_LOG_WARN(
        absl::StrCat("type name collision detected while registering "
                     "attribute, ignoring: type '",
                     type.name(), "' previously from ", *registered_source));
    } else {
      IRS_LOG_WARN(
        absl::StrCat("type name collision detected while registering "
                     "attribute, ignoring: type '",
                     type.name(), "'"));
    }
  }
}

attribute_registrar::operator bool() const noexcept { return registered_; }

}  // namespace irs
