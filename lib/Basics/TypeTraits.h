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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_TYPETRAITS_H
#define ARANGODB_BASICS_TYPETRAITS_H

namespace arangodb {

template <typename Base, typename Derived, typename = void>
struct can_static_cast : std::false_type {};

template <typename Base, typename Derived>
struct can_static_cast<Base, Derived, std::void_t<decltype(static_cast<Derived*>((Base*)nullptr))>>
    : std::true_type {};

template <typename Base, typename Derived>
constexpr bool can_static_cast_v = can_static_cast<Base, Derived>::value;

}  // namespace arangodb

#endif  // ARANGODB_BASICS_TYPETRAITS_H
