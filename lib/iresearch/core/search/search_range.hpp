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

#pragma once

#include "utils/hash_utils.hpp"
#include "utils/type_utils.hpp"

namespace irs {

enum class BoundType {
  UNBOUNDED,
  INCLUSIVE,
  EXCLUSIVE,
};

template<typename T>
struct search_range {
  T min{};
  T max{};
  BoundType min_type = BoundType::UNBOUNDED;
  BoundType max_type = BoundType::UNBOUNDED;

  bool operator==(const search_range& rhs) const noexcept {
    return min == rhs.min && min_type == rhs.min_type && max == rhs.max &&
           max_type == rhs.max_type;
  }

  bool operator!=(const search_range& rhs) const noexcept {
    return !(*this == rhs);
  }
};

}  // namespace irs
