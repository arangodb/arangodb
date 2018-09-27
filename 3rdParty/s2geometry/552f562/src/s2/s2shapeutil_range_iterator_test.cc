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

#include <gtest/gtest.h>
#include "s2/mutable_s2shape_index.h"
#include "s2/s2text_format.h"

namespace s2shapeutil {

TEST(RangeIterator, Next) {
  // Create an index with one point each on S2CellId faces 0, 1, and 2.
  auto index = s2textformat::MakeIndex("0:0 | 0:90 | 90:0 # #");
  RangeIterator it(*index);
  EXPECT_EQ(0, it.id().face());
  it.Next();
  EXPECT_EQ(1, it.id().face());
  it.Next();
  EXPECT_EQ(2, it.id().face());
  it.Next();
  EXPECT_EQ(S2CellId::Sentinel(), it.id());
  EXPECT_TRUE(it.done());
}

TEST(RangeIterator, EmptyIndex) {
  auto empty = s2textformat::MakeIndex("# #");
  auto non_empty = s2textformat::MakeIndex("0:0 # #");
  RangeIterator empty_it(*empty);
  RangeIterator non_empty_it(*non_empty);
  EXPECT_FALSE(non_empty_it.done());
  EXPECT_TRUE(empty_it.done());

  empty_it.SeekTo(non_empty_it);
  EXPECT_TRUE(empty_it.done());

  empty_it.SeekBeyond(non_empty_it);
  EXPECT_TRUE(empty_it.done());

  empty_it.SeekTo(empty_it);
  EXPECT_TRUE(empty_it.done());

  empty_it.SeekBeyond(empty_it);
  EXPECT_TRUE(empty_it.done());
}

}  // namespace s2shapeutil
