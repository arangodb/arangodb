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

#ifndef IRESEARCH_TYPE_ID_H
#define IRESEARCH_TYPE_ID_H

#include "type_info.hpp"
#include "misc.hpp"

namespace iresearch {

namespace detail {
DEFINE_HAS_MEMBER(type_name);
} // detail

////////////////////////////////////////////////////////////////////////////////
/// @class type
/// @tparam T type for which one needs access meta information
/// @brief convenient helper for accessing meta information
////////////////////////////////////////////////////////////////////////////////
template<typename T>
struct type {
  //////////////////////////////////////////////////////////////////////////////
  /// @returns an instance of "type_info" object holding meta information of
  ///          type denoted by template parameter "T"
  //////////////////////////////////////////////////////////////////////////////
  static constexpr type_info get() noexcept {
    return type_info{id(), name()};
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns type name of a type denoted by template parameter "T"
  /// @note Do never persist type name provided by detail::ctti<T> as
  ///       it's platform dependent
  //////////////////////////////////////////////////////////////////////////////
  static constexpr string_ref name() noexcept {
    if constexpr (detail::has_member_type_name_v<T>) {
      return T::type_name();
    }

    return ctti<T>();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns type identifier of a type denoted by template parameter "T"
  //////////////////////////////////////////////////////////////////////////////
  static constexpr type_info::type_id id() noexcept {
    return &get;
  }
}; // type

}

#endif
