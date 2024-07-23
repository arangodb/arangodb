////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

namespace arangodb::containers {
namespace detail {

template<typename T>
char isCompleteHelper(char (*)[sizeof(T)]);

template<typename T>
int isCompleteHelper(...);

template<size_t Sizeof, typename T>
constexpr bool SetSizeofChecker() noexcept {
  if constexpr (sizeof(isCompleteHelper<T>(nullptr)) == sizeof(char)) {
    static_assert(sizeof(T) <= Sizeof,
                  "For large T better to use NodeHashSet. "
                  "If you really sure what do you do,"
                  " please use absl::flat_hash_set directly.");
  }
  return true;
}

}  // namespace detail

template<class T, class Hash = typename absl::flat_hash_set<T>::hasher,
         class Eq = typename absl::flat_hash_set<T, Hash>::key_equal,
         class Allocator =
             typename absl::flat_hash_set<T, Hash, Eq>::allocator_type
#if !defined(ABSL_HAVE_ADDRESS_SANITIZER) && \
    !defined(ABSL_HAVE_MEMORY_SANITIZER)
         ,  // TODO(MBkkt) After additional benchmarks change Sizeof
         class = std::enable_if_t<detail::SetSizeofChecker<40, T>()>
#endif
         >
using FlatHashSet = absl::flat_hash_set<T, Hash, Eq, Allocator>;

}  // namespace arangodb::containers
