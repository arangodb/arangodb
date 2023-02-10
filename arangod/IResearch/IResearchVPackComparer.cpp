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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchVPackComparer.h"
#include "IResearchViewSort.h"
#include "IResearchInvertedIndexMeta.h"

#include "Basics/VelocyPackHelper.h"

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief reverse multiplier to use in VPackComparer
////////////////////////////////////////////////////////////////////////////////
constexpr int kMultiplier[]{-1, 1};

}  // namespace

namespace arangodb::iresearch {

template<typename Sort>
VPackComparer<Sort>::VPackComparer() : _sort{nullptr}, _size{0} {}

template<typename Sort>
int VPackComparer<Sort>::CompareImpl(irs::bytes_view lhs,
                                     irs::bytes_view rhs) const {
  TRI_ASSERT(_sort);
  TRI_ASSERT(_sort->size() >= _size);
  TRI_ASSERT(!lhs.empty());
  TRI_ASSERT(!rhs.empty());

  auto const* lhsStart = lhs.data();
  auto const* rhsStart = rhs.data();
  for (size_t i = 0; i < _size; ++i) {
    velocypack::Slice lhsSlice{lhsStart};
    velocypack::Slice rhsSlice{rhsStart};
    TRI_ASSERT(!lhsSlice.isNone());
    TRI_ASSERT(!rhsSlice.isNone());

    auto const r = basics::VelocyPackHelper::compare(lhsSlice, rhsSlice, true);
    if (r) {
      return kMultiplier[size_t{_sort->direction(i)}] * r;
    }

    // move to the next value
    lhsStart += lhsSlice.byteSize();
    rhsStart += rhsSlice.byteSize();
  }
  return 0;
}

template class VPackComparer<IResearchSortBase>;
template class VPackComparer<IResearchViewSort>;
template class VPackComparer<IResearchInvertedIndexSort>;

}  // namespace arangodb::iresearch
