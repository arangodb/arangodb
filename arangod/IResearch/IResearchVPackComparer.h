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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IResearch/IResearchViewSort.h"

#include "index/comparer.hpp"
#include "utils/string.hpp"

namespace arangodb::iresearch {

template<typename Sort>
class VPackComparer final : public irs::Comparer {
 public:
  VPackComparer();

  explicit VPackComparer(Sort const& sort) noexcept
      : _sort{&sort}, _size{sort.size()} {}

  VPackComparer(Sort const& sort, size_t size) noexcept
      : _sort{&sort}, _size{std::min(sort.size(), size)} {}

  void reset(Sort const& sort) noexcept {
    _sort = &sort;
    _size = sort.size();
  }

  bool empty() const noexcept { return 0 == _size; }

 private:
  int CompareImpl(irs::bytes_view lhs, irs::bytes_view rhs) const final;

  Sort const* _sort;
  size_t _size;  // number of buckets to compare
};

}  // namespace arangodb::iresearch
