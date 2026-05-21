////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#include "utils/memory.hpp"
#include "utils/type_id.hpp"

namespace irs {

// Base struct for all attribute types that can be used with attribute_provider.
struct attribute {};

// Base class for all objects with externally visible attributes
struct attribute_provider : memory::Managed {
  // Return pointer to attribute of a specified type.
  // External users should prefer using const version.
  // External users should avoid modifying attributes treat that as UB.
  virtual attribute* get_mutable(type_info::type_id type) = 0;

  // Const pointer to attribute of a specified type.
  const attribute* get(type_info::type_id type) const {
    return const_cast<attribute_provider*>(this)->get_mutable(type);
  }
};

// Convenient helper for getting immutable attribute of a specific type.
template<typename T, typename Provider,
         typename = std::enable_if_t<std::is_base_of_v<attribute, T>>>
inline const T* get(const Provider& attrs) {
  return static_cast<const T*>(attrs.get(type<T>::id()));
}

// Convenient helper for getting mutable attribute of a specific type.
template<typename T, typename Provider,
         typename = std::enable_if_t<std::is_base_of_v<attribute, T>>>
inline T* get_mutable(Provider* attrs) {
  return static_cast<T*>(attrs->get_mutable(type<T>::id()));
}

}  // namespace irs
