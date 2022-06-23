////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string_view>
#include <type_traits>

#include <velocypack/HashedStringRef.h>
#include <velocypack/SharedSlice.h>
#include <velocypack/Slice.h>

#include "Inspection/Status.h"

namespace arangodb::inspection {
template<class T>
struct Access;

template<class T>
struct AccessBase;
}  // namespace arangodb::inspection

namespace arangodb::inspection::detail {

template<class T, class Inspector, class = void>
struct HasInspectOverload : std::false_type {};

template<class T, class Inspector>
struct HasInspectOverload<T, Inspector,
                          std::void_t<decltype(inspect(
                              std::declval<Inspector&>(), std::declval<T&>()))>>
    : std::conditional_t<
          std::is_convertible_v<decltype(inspect(std::declval<Inspector&>(),
                                                 std::declval<T&>())),
                                Status>,
          std::true_type, typename std::false_type> {};

template<class T>
constexpr inline bool IsSafeBuiltinType() {
  return std::is_same_v<T, bool> || std::is_integral_v<T> ||
         std::is_floating_point_v<T> ||
         std::is_same_v<T, std::string> ||  // TODO - use is-string-like?
         std::is_same_v<T, arangodb::velocypack::SharedSlice>;
}

template<class T>
constexpr inline bool IsUnsafeBuiltinType() {
  return std::is_same_v<T, std::string_view> ||
         std::is_same_v<T, arangodb::velocypack::Slice> ||
         std::is_same_v<T, arangodb::velocypack::HashedStringRef>;
}

template<class T>
constexpr inline bool IsBuiltinType() {
  return IsSafeBuiltinType<T>() || IsUnsafeBuiltinType<T>();
}

template<class T, class = void>
struct IsListLike : std::false_type {};

template<class T>
struct IsListLike<T, std::void_t<decltype(std::declval<T>().begin() !=
                                          std::declval<T>().end()),
                                 decltype(++std::declval<T>().begin()),
                                 decltype(*std::declval<T>().begin()),
                                 decltype(std::declval<T>().push_back(
                                     std::declval<typename T::value_type>()))>>
    : std::true_type {};

template<class T, class = void>
struct IsMapLike : std::false_type {};

template<class T>
struct IsMapLike<T, std::void_t<decltype(std::declval<T>().begin() !=
                                         std::declval<T>().end()),
                                decltype(++std::declval<T>().begin()),
                                decltype(*std::declval<T>().begin()),
                                typename T::key_type, typename T::mapped_type,
                                decltype(std::declval<T>().emplace(
                                    std::declval<std::string>(),
                                    std::declval<typename T::mapped_type>()))>>
    : std::true_type {};

template<class T, class = void>
struct IsTuple
    : std::conditional_t<std::is_array_v<T>, std::true_type, std::false_type> {
};

template<class T>
struct IsTuple<T, std::void_t<typename std::tuple_size<T>::type>>
    : std::true_type {};

template<class T, std::size_t = sizeof(T)>
std::true_type IsCompleteTypeImpl(T*);

std::false_type IsCompleteTypeImpl(...);

template<class T>
constexpr inline bool IsCompleteType() {
  return decltype(IsCompleteTypeImpl(std::declval<T*>()))::value;
}

template<class T>
constexpr inline bool HasAccessSpecialization() {
  return IsCompleteType<Access<T>>();
}

template<class T, class Inspector>
constexpr inline bool IsInspectable() {
  return IsBuiltinType<T>() || HasInspectOverload<T, Inspector>::value ||
         IsListLike<T>::value || IsMapLike<T>::value || IsTuple<T>::value ||
         HasAccessSpecialization<T>();
}

template<class T>
using AccessType = std::conditional_t<detail::HasAccessSpecialization<T>(),
                                      Access<T>, AccessBase<T>>;

}  // namespace arangodb::inspection::detail
