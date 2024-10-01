// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "s2/s2point_index.h"

#include <set>

#include <gtest/gtest.h>
#include "s2/s2cell_id.h"
#include "s2/s2cell_union.h"
#include "s2/s2testing.h"

class S2PointIndexTest : public ::testing::Test {
 protected:
  using Index = S2PointIndex<int>;
  using PointData = Index::PointData;
  using Contents = std::multiset<PointData>;
  Index index_;
  Contents contents_;

 public:
  void Add(const S2Point& point, int data) {
    index_.Add(point, data);
    contents_.insert(PointData(point, data));
  }

  void Remove(const S2Point& point, int data) {
    // If there are multiple copies, remove only one.
    contents_.erase(contents_.find(PointData(point, data)));
    index_.Remove(point, data);  // Invalidates "point".
  }

  void Verify() {
    VerifyContents();
    VerifyIteratorMethods();
  }

  void VerifyContents() {
    Contents remaining = contents_;
    for (Index::Iterator it(&index_); !it.done(); it.Next()) {
      Contents::iterator element = remaining.find(it.point_data());
      EXPECT_TRUE(element != remaining.end());
      remaining.erase(element);
    }
    EXPECT_TRUE(remaining.empty());
  }

  void VerifyIteratorMethods() {
    Index::Iterator it(&index_);
    EXPECT_FALSE(it.Prev());
    it.Finish();
    EXPECT_TRUE(it.done());

    // Iterate through all the cells in the index.
    S2CellId prev_cellid = S2CellId::None();
    S2CellId min_cellid = S2CellId::Begin(S2CellId::kMaxLevel);
    for (it.Begin(); !it.done(); it.Next()) {
      S2CellId cellid = it.id();
      EXPECT_EQ(cellid, S2CellId(it.point()));
      EXPECT_GE(cellid, prev_cellid);

      typename Index::Iterator it2(&index_);
      if (cellid == prev_cellid) {
        it2.Seek(cellid);
      }

      // Generate a cellunion that covers the range of empty leaf cells between
      // the last cell and this one.  Then make sure that seeking to any of
      // those cells takes us to the immediately following cell.
      if (cellid > prev_cellid) {
        for (S2CellId skipped : S2CellUnion::FromBeginEnd(min_cellid, cellid)) {
          it2.Seek(skipped);
          EXPECT_EQ(cellid, it2.id());
        }
      }
      // Test Prev(), Next(), and Seek().
      if (prev_cellid.is_valid()) {
        it2 = it;
        EXPECT_TRUE(it2.Prev());
        EXPECT_EQ(prev_cellid, it2.id());
        it2.Next();
        EXPECT_EQ(cellid, it2.id());
        it2.Seek(prev_cellid);
        EXPECT_EQ(prev_cellid, it2.id());
      }
      prev_cellid = cellid;
      min_cellid = cellid.next();
    }
  }
};

TEST_F(S2PointIndexTest, NoPoints) {
  Verify();
}

TEST_F(S2PointIndexTest, DuplicatePoints) {
  for (int i = 0; i < 10; ++i) {
    Add(S2Point(1, 0, 0), 123);  // All points have same Data argument.
  }
  Verify();
  // Now remove half of the points.
  for (int i = 0; i < 5; ++i) {
    Remove(S2Point(1, 0, 0), 123);
  }
  Verify();
}

TEST_F(S2PointIndexTest, RandomPoints) {
  for (int i = 0; i < 100; ++i) {
    Add(S2Testing::RandomPoint(), S2Testing::rnd.Uniform(100));
  }
  Verify();
  // Now remove some of the points.
  for (int i = 0; i < 10; ++i) {
    S2PointIndex<int>::Iterator it(&index_);
    do {
      it.Seek(S2Testing::GetRandomCellId(S2CellId::kMaxLevel));
    } while (it.done());
    Remove(it.point(), it.data());
    Verify();
  }
}

TEST(S2PointIndex, EmptyData) {
  // Verify that when Data is an empty class, no space is used.
  EXPECT_EQ(sizeof(S2Point), sizeof(S2PointIndex<>::PointData));

  // Verify that points can be added and removed with an empty Data class.
  S2PointIndex<> index;
  index.Add(S2Point(1, 0, 0));
  index.Remove(S2Point(1, 0, 0));
  EXPECT_EQ(0, index.num_points());
}
