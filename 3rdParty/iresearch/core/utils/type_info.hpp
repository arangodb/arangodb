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

#ifndef IRESEARCH_TYPE_INDEX_H
#define IRESEARCH_TYPE_INDEX_H

#include "shared.hpp"
#include "string.hpp"

namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @class type_info
/// @brief holds meta information obout a type, e.g. name and identifier
////////////////////////////////////////////////////////////////////////////////
class type_info {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief unique type identifier
  /// @note can be used to get an instance of underlying type
  //////////////////////////////////////////////////////////////////////////////
  using type_id = type_info(*)() noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief default constructor produces invalid type identifier
  //////////////////////////////////////////////////////////////////////////////
  constexpr type_info() noexcept
    : type_info(nullptr, string_ref::NIL) {
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @return true if type_info is valid, false - otherwise
  //////////////////////////////////////////////////////////////////////////////
  constexpr explicit operator bool() const noexcept {
    return nullptr != id_;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @return true if current object is equal to a denoted by 'rhs'
  //////////////////////////////////////////////////////////////////////////////
  constexpr bool operator==(const type_info& rhs) const noexcept {
    return id_ == rhs.id_;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @return true if current object is not equal to a denoted by 'rhs'
  //////////////////////////////////////////////////////////////////////////////
  constexpr bool operator!=(const type_info& rhs) const noexcept {
    return !(*this == rhs);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @return true if current object is less than to a denoted by 'rhs'
  //////////////////////////////////////////////////////////////////////////////
  bool operator<(const type_info& rhs) const noexcept {
    return id_ < rhs.id_;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @return type name
  //////////////////////////////////////////////////////////////////////////////
  constexpr const string_ref& name() const noexcept { return name_; }

  //////////////////////////////////////////////////////////////////////////////
  /// @return type identifier
  //////////////////////////////////////////////////////////////////////////////
  constexpr type_id id() const noexcept { return id_; }

 private:
  template<typename T>
  friend struct type;

  constexpr type_info(type_id id, const string_ref& name) noexcept
    : id_(id), name_(name) {
  }

  type_id id_;
  string_ref name_;
}; // type_info

}

namespace std {

template<>
struct hash<::iresearch::type_info> {
  size_t operator()(const ::iresearch::type_info& key) const {
    return std::hash<decltype(key.id())>()(key.id());
  }
};

} // std

#endif // IRESEARCH_TYPE_INDEX_H


