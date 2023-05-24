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

#include <absl/container/flat_hash_set.h>
namespace std::pmr {
template<class T>
class polymorphic_allocator;
}

namespace arangodb::containers {
namespace detail {

template<typename T>
char isCompleteHelper(char (*)[sizeof(T)]);

template<typename T>
int isCompleteHelper(...);

template<size_t SizeofT, typename T>
constexpr bool SetSizeofChecker() noexcept {
  if constexpr (sizeof(decltype(isCompleteHelper<T>(nullptr))) ==
                sizeof(char)) {
    static_assert(sizeof(T) <= SizeofT,
                  "For large T better to use NodeHashSet. "
                  "If you really sure what do you do,"
                  " please use absl::flat_hash_set directly.");
    return sizeof(T) <= SizeofT;
  } else {
    return true;
  }
}

}  // namespace detail

template<class T, class Hash = typename absl::flat_hash_set<T>::hasher,
         class Eq = typename absl::flat_hash_set<T, Hash>::key_equal,
         class Allocator =
             typename absl::flat_hash_set<T, Hash, Eq>::allocator_type,
         // TODO(MBkkt) After additional benchmarks make SizeofT bigger
         class = std::enable_if<detail::SetSizeofChecker<32, T>(), void>>
using FlatHashSet = absl::flat_hash_set<T, Hash, Eq, Allocator>;

namespace pmr {
template<class T, class Hash = typename absl::flat_hash_set<T>::hasher,
         class Eq = typename absl::flat_hash_set<T, Hash>::key_equal>
using FlatHashSet =
    arangodb::containers::FlatHashSet<T, Hash, Eq,
                                      std::pmr::polymorphic_allocator<T>>;
}

}  // namespace arangodb::containers
