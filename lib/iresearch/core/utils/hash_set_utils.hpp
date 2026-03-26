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

#pragma once

#include "hash_utils.hpp"

#include <absl/container/flat_hash_set.h>

namespace irs {

template<typename T>
struct ValueRef {
  explicit ValueRef(T ref, size_t hash) : ref{ref}, hash{hash} {}

  T ref;
  size_t hash;
};

struct ValueRefHash {
  using is_transparent = void;

  template<typename T>
  size_t operator()(const ValueRef<T>& value) const noexcept {
    return value.hash;
  }

  template<typename Char>
  size_t operator()(
    const hashed_basic_string_view<Char>& value) const noexcept {
    return value.hash();
  }
};

template<typename T>
struct ValueRefEq {
  using Self = ValueRefEq<T>;
  using Ref = ValueRef<T>;

  bool operator()(const Ref& lhs, const Ref& rhs) const noexcept {
    return lhs.ref == rhs.ref;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Abseil hash containers behave in a way that in presence of removals
///        rehash may still happen even if enough space was allocated
////////////////////////////////////////////////////////////////////////////////
template<typename Eq>
using flat_hash_set = absl::flat_hash_set<typename Eq::Ref, ValueRefHash, Eq>;

}  // namespace irs
