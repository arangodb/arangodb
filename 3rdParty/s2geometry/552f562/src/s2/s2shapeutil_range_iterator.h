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

#ifndef S2_S2SHAPEUTIL_RANGE_ITERATOR_H_
#define S2_S2SHAPEUTIL_RANGE_ITERATOR_H_

#include "s2/s2cell_id.h"
#include "s2/s2shape_index.h"

class S2Loop;
class S2Error;

namespace s2shapeutil {

// RangeIterator is a wrapper over S2ShapeIndex::Iterator with extra methods
// that are useful for merging the contents of two or more S2ShapeIndexes.
class RangeIterator {
 public:
  // Construct a new RangeIterator positioned at the first cell of the index.
  explicit RangeIterator(const S2ShapeIndex& index);

  // The current S2CellId and cell contents.
  S2CellId id() const { return it_.id(); }
  const S2ShapeIndexCell& cell() const { return it_.cell(); }

  // The min and max leaf cell ids covered by the current cell.  If done() is
  // true, these methods return a value larger than any valid cell id.
  S2CellId range_min() const { return range_min_; }
  S2CellId range_max() const { return range_max_; }

  void Next();
  bool done() { return it_.done(); }

  // Position the iterator at the first cell that overlaps or follows
  // "target", i.e. such that range_max() >= target.range_min().
  void SeekTo(const RangeIterator& target);

  // Position the iterator at the first cell that follows "target", i.e. the
  // first cell such that range_min() > target.range_max().
  void SeekBeyond(const RangeIterator& target);

 private:
  // Updates internal state after the iterator has been repositioned.
  void Refresh();
  S2ShapeIndex::Iterator it_;
  S2CellId range_min_, range_max_;
};

}  // namespace s2shapeutil

#endif  // S2_S2SHAPEUTIL_RANGE_ITERATOR_H_
