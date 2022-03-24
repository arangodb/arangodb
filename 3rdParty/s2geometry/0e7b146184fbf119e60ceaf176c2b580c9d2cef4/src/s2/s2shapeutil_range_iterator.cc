// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// Author: ericv@google.com (Eric Veach)

#include "s2/s2shapeutil_range_iterator.h"

namespace s2shapeutil {

RangeIterator::RangeIterator(const S2ShapeIndex& index)
    : it_(&index, S2ShapeIndex::BEGIN) {
  Refresh();
}

void RangeIterator::Next() {
  it_.Next();
  Refresh();
}

void RangeIterator::SeekTo(const RangeIterator& target) {
  it_.Seek(target.range_min());
  // If the current cell does not overlap "target", it is possible that the
  // previous cell is the one we are looking for.  This can only happen when
  // the previous cell contains "target" but has a smaller S2CellId.
  if (it_.done() || it_.id().range_min() > target.range_max()) {
    if (it_.Prev() && it_.id().range_max() < target.id()) it_.Next();
  }
  Refresh();
}

void RangeIterator::SeekBeyond(const RangeIterator& target) {
  it_.Seek(target.range_max().next());
  if (!it_.done() && it_.id().range_min() <= target.range_max()) {
    it_.Next();
  }
  Refresh();
}

// This method is inline, but is only called by non-inline methods defined in
// this file.  Putting the definition here enforces this requirement.
inline void RangeIterator::Refresh() {
  range_min_ = id().range_min();
  range_max_ = id().range_max();
}

}  // namespace s2shapeutil
