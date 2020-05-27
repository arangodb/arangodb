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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////


#ifndef ARANGODB_BASICS_OVERLOAD_H
#define ARANGODB_BASICS_OVERLOAD_H 1

namespace arangodb {

/**
 * @brief Construct an overloaded callable from multiple callables.
 */
template<class... Ts> struct overload : Ts... { using Ts::operator()...; };
template<class... Ts> overload(Ts...) -> overload<Ts...>;

/*
 * Usage example:
 *
 * std::variant<int, float> intOrFloat { 0.0f };
 * std::visit(overload {
 *     [](int const& i) { ... },
 *     [](float const& f) { ... },
 *   },
 *   intOrFloat;
 * );
  */
}

#endif // ARANGODB_BASICS_OVERLOAD_H
