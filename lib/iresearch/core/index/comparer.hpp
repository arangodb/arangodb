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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "shared.hpp"
#include "utils/string.hpp"

namespace irs {

class Comparer {
 public:
  virtual ~Comparer() = default;

  int Compare(bytes_view lhs, bytes_view rhs) const {
    IRS_ASSERT(!IsNull(lhs));
    IRS_ASSERT(!IsNull(rhs));
    const auto r = CompareImpl(lhs, rhs);
#ifdef IRESEARCH_DEBUG
    // Comparator validity check
    const auto r1 = CompareImpl(rhs, lhs);
    IRS_ASSERT((r == 0 && r1 == 0) || (r * r1 < 0));
#endif
    return r;
  }

 protected:
  virtual int CompareImpl(bytes_view lhs, bytes_view rhs) const = 0;
};

inline bool UseDenseSort(size_t size, size_t total) noexcept {
  // check: N*logN > K
  return std::isgreaterequal(static_cast<double_t>(size) * std::log(size),
                             static_cast<double_t>(total));
}

}  // namespace irs
