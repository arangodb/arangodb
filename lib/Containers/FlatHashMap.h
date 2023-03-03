////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <absl/container/flat_hash_map.h>

namespace arangodb::containers {
namespace detail {

template<typename T>
char isCompleteHelper(char (*)[sizeof(T)]);

template<typename T>
int isCompleteHelper(...);

template<size_t SizeofK, size_t SizeofV, typename K, typename V>
constexpr bool MapSizeofChecker() noexcept {
  if constexpr (sizeof(decltype(isCompleteHelper<K>(nullptr))) +
                    sizeof(decltype(isCompleteHelper<V>(nullptr))) ==
                2 * sizeof(char)) {
    static_assert(sizeof(K) <= SizeofK && sizeof(V) <= SizeofV,
                  "For large K better to use NodeHashMap. "
                  "For large V better to use FlatHashMap<K, unique_ptr<V>>. "
                  "If you really sure what do you do,"
                  " please use absl::flat_hash_map directly.");
    return sizeof(K) <= SizeofK && sizeof(V) <= SizeofV;
  } else {
    return true;
  }
}

}  // namespace detail

template<class K, class V,
         class Hash = typename absl::flat_hash_map<K, V>::hasher,
         class Eq = typename absl::flat_hash_map<K, V, Hash>::key_equal,
         class Allocator =
             typename absl::flat_hash_map<K, V, Hash, Eq>::allocator_type,
         // TODO(MBkkt) After additional benchmarks make SizeofV smaller
         class = std::enable_if<detail::MapSizeofChecker<32, 64, K, V>(), void>>
using FlatHashMap = absl::flat_hash_map<K, V, Hash, Eq, Allocator>;

}  // namespace arangodb::containers
