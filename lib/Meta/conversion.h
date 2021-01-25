////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_META_CONVERSION_H
#define ARANGODB_META_CONVERSION_H 1

#include <type_traits>

namespace arangodb {
namespace meta {
namespace details {

template <typename E>
using enable_enum_t =
    typename std::enable_if<std::is_enum<E>::value, typename std::underlying_type<E>::type>::type;
}

template <typename E>
constexpr details::enable_enum_t<E> underlyingValue(E e) noexcept {
  return static_cast<typename std::underlying_type<E>::type>(e);
}

template <typename E, typename T>
constexpr typename std::enable_if<std::is_enum<E>::value && std::is_integral<T>::value, E>::type toEnum(
    T value) noexcept {
  return static_cast<E>(value);
}

template <typename E_OUT, typename E_IN>
constexpr
    typename std::enable_if<std::is_enum<E_IN>::value && std::is_enum<E_OUT>::value, E_OUT>::type
    enumToEnum(E_IN value) noexcept {
  return toEnum<E_OUT>(underlyingValue(value));
}
}  // namespace meta
}  // namespace arangodb

#endif
