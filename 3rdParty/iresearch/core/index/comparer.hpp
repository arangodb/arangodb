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

#ifndef IRESEARCH_COMPARER_H
#define IRESEARCH_COMPARER_H

#include "shared.hpp"
#include "utils/string.hpp"

NS_ROOT

class comparer {
 public:
  virtual ~comparer() = default;

  bool operator()(const bytes_ref& lhs, const bytes_ref& rhs) const {
    return less(lhs, rhs);
  }

 protected:
  virtual bool less(const bytes_ref& lhs, const bytes_ref& rhs) const = 0;
}; // comparer

inline bool use_dense_sort(size_t size, size_t total) noexcept {
  // check: N*logN > K
  return std::isgreaterequal(
    static_cast<double_t>(size)*std::log(size),
    static_cast<double_t>(total)
  );
}

NS_END

#endif // IRESEARCH_COMPARER_H

