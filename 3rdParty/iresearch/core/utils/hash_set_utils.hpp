////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef IRESEARCH_HASH_CONTAINER_UTILS
#define IRESEARCH_HASH_CONTAINER_UTILS

#include <absl/container/flat_hash_set.h>

#include "hash_utils.hpp"

namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @brief first - hash value, second - reference
////////////////////////////////////////////////////////////////////////////////
template<typename T>
using value_ref_t = std::pair<size_t, T>;

////////////////////////////////////////////////////////////////////////////////
/// @struct transparent hash for value_ref_t
////////////////////////////////////////////////////////////////////////////////
class value_ref_hash {
 public:
  using is_transparent = void;

  template<typename T>
  size_t operator()(const value_ref_t<T>& value) const noexcept {
    return value.first;
  }

  template<typename Char>
  size_t operator()(const hashed_basic_string_ref<Char>& value) const noexcept {
    return value.hash();
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @struct transparent equality comparator for value_ref_t
////////////////////////////////////////////////////////////////////////////////
template<typename T>
struct value_ref_eq {
  using is_transparent = void;

  using self_t = value_ref_eq<T>;
  using ref_t = value_ref_t<T>;
  using value_t = typename ref_t::second_type;

  bool operator()(const ref_t& lhs, const ref_t& rhs) const noexcept {
    return lhs.second == rhs.second;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Abseil hash containers behave in a way that in presence of removals
///        rehash may still happen even if enough space was allocated
////////////////////////////////////////////////////////////////////////////////
template<typename Eq>
using flat_hash_set = absl::flat_hash_set<typename Eq::ref_t, value_ref_hash, Eq>;

}

#endif// IRESEARCH_HASH_CONTAINER_UTILS
