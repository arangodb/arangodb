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

#include "s2/s2shapeutil_count_edges.h"

#include <gtest/gtest.h>
#include "s2/mutable_s2shape_index.h"
#include "s2/s2text_format.h"

namespace {

TEST(CountEdgesUpTo, StopsEarly) {
  auto index = s2textformat::MakeIndex(
      "0:0 | 0:1 | 0:2 | 0:3 | 0:4 # 1:0, 1:1 | 1:2, 1:3 | 1:4, 1:5, 1:6 #");
  // Verify the test parameters.
  ASSERT_EQ(index->num_shape_ids(), 4);
  ASSERT_EQ(index->shape(0)->num_edges(), 5);
  ASSERT_EQ(index->shape(1)->num_edges(), 1);
  ASSERT_EQ(index->shape(2)->num_edges(), 1);
  ASSERT_EQ(index->shape(3)->num_edges(), 2);

  EXPECT_EQ(s2shapeutil::CountEdges(*index), 9);
  EXPECT_EQ(s2shapeutil::CountEdgesUpTo(*index, 1), 5);
  EXPECT_EQ(s2shapeutil::CountEdgesUpTo(*index, 5), 5);
  EXPECT_EQ(s2shapeutil::CountEdgesUpTo(*index, 6), 6);
  EXPECT_EQ(s2shapeutil::CountEdgesUpTo(*index, 8), 9);
}

}  // namespace
