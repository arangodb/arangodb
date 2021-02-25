////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef IRESEARCH_SEARCH_RANGE_H
#define IRESEARCH_SEARCH_RANGE_H

#include "utils/hash_utils.hpp"

namespace iresearch {

enum class BoundType {
  UNBOUNDED,
  INCLUSIVE,
  EXCLUSIVE
};

template<typename T>
struct search_range {
  T min{};
  T max{};
  BoundType min_type = BoundType::UNBOUNDED;
  BoundType max_type = BoundType::UNBOUNDED;

  size_t hash() const noexcept {
    using bound_type = typename std::underlying_type<BoundType>::type;

    const auto hash0 = hash_combine(
      std::hash<decltype(min)>()(min),
      std::hash<decltype(max)>()(max));
    const auto hash1 = hash_combine(
      std::hash<bound_type>()(static_cast<bound_type>(min_type)),
      std::hash<bound_type>()(static_cast<bound_type>(max_type)));
    return hash_combine(hash0, hash1);
  }

  bool operator==(const search_range& rhs) const noexcept {
    return min == rhs.min && min_type == rhs.min_type
      && max == rhs.max && max_type == rhs.max_type;
  }

  bool operator!=(const search_range& rhs) const noexcept {
    return !(*this == rhs);
  }
}; // search_range

}

namespace std {

template<typename T>
struct hash<::iresearch::search_range<T>> {
  size_t operator()(const ::iresearch::search_range<T>& value) {
    return value.hash();
  }
};

}

#endif // IRESEARCH_SEARCH_RANGE_H
