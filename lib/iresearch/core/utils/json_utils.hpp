////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "string.hpp"

namespace irs {

template<typename GenericObject>
inline bool get_uint64(const GenericObject& json, std::string_view name,
                       uint64_t& value) {
  if (!json.HasMember(name.c_str())) {
    return false;
  }

  const auto& attr = json[name.c_str()];

  if (!attr.IsNumber()) {
    return false;
  }

  value = attr.GetUint64();
  return true;
}

template<typename GenericObject>
inline bool get_bool(const GenericObject& json, std::string_view name,
                     bool& value) {
  if (!json.HasMember(name.c_str())) {
    return false;
  }

  const auto& attr = json[name.c_str()];

  if (!attr.IsBool()) {
    return false;
  }

  value = attr.GetBool();
  return true;
}

}  // namespace irs
