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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IRESEARCH__IRESEARCH_VPACK_COMPARER_H
#define ARANGODB_IRESEARCH__IRESEARCH_VPACK_COMPARER_H 1

#include "IResearchViewSort.h"
#include "index/comparer.hpp"
#include "utils/string.hpp"

namespace arangodb {
namespace iresearch {

class VPackComparer final : public irs::comparer {
 public:
  VPackComparer();

  explicit VPackComparer(IResearchViewSort const& sort) noexcept
    : _sort(&sort), _size(sort.size()) {
  }

  VPackComparer(IResearchViewSort const& sort, size_t size) noexcept
    : _sort(&sort), _size(std::min(sort.size(), size)) {
  }

  void reset(IResearchViewSort const& sort) noexcept {
    _sort = &sort;
    _size = sort.size();
  }

  bool empty() const noexcept {
    return 0 == _size;
  }

 protected:
  virtual bool less(irs::bytes_ref const& lhs,
                    irs::bytes_ref const& rhs) const override;

 private:
  IResearchViewSort const* _sort;
  size_t _size; // number of buckets to compare
}; // VPackComparer

} // iresearch
} // arangodb

#endif // ARANGODB_IRESEARCH__IRESEARCH_VPACK_COMPARER_H
