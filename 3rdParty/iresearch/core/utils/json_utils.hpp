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

#ifndef IRESEARCH_JSON_UTILS_H
#define IRESEARCH_JSON_UTILS_H

#include "string.hpp"

namespace iresearch {

template<typename GenericObject>
inline bool get_uint64(GenericObject const& json,
                       const irs::string_ref& name,
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
inline bool get_bool(GenericObject const& json,
                     const irs::string_ref& name,
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

}

#endif // IRESEARCH_JSON_UTILS_H
