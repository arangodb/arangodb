// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "s2/s2cell_index.h"

#include <set>
#include <utility>
#include <vector>
#include "s2/base/stringprintf.h"
#include <gtest/gtest.h>
#include "s2/s2cell_id.h"
#include "s2/s2cell_union.h"
#include "s2/s2testing.h"

using std::pair;
using std::set;
using std::vector;

using Label = S2CellIndex::Label;
using LabelledCell = S2CellIndex::LabelledCell;

namespace {

class S2CellIndexTest : public ::testing::Test {
 public:
  // Adds the (cell_id, label) pair to index_ and also contents_ (which is
  // used for independent validation).
  void Add(S2CellId cell_id, Label label) {
    index_.Add(cell_id, label);
    contents_.push_back(LabelledCell(cell_id, label));
  }

  void Add(const string& cell_str, Label label) {
    Add(S2CellId::FromDebugString(cell_str), label);
  }

  void Add(const S2CellUnion& cell_union, Label label) {
    index_.Add(cell_union, label);
    for (S2CellId cell_id : cell_union) {
      contents_.push_back(LabelledCell(cell_id, label));
    }
  }

  void Build() { index_.Build(); }

  void QuadraticValidate();
  void VerifyCellIterator() const;
  void VerifyRangeIterators() const;
  void VerifyIndexContents() const;
  void TestIntersection(const S2CellUnion& target);
  void ExpectContents(const string& target_str,
                      S2CellIndex::ContentsIterator* contents,
                      const vector<pair<string, Label>>& expected_strs) const;

 protected:
  S2CellIndex index_;
  vector<LabelledCell> contents_;
};

// Verifies that the index computes the correct set of (cell_id, label) pairs
// for every possible leaf cell.  The running time of this function is
// quadratic in the size of the index.
void S2CellIndexTest::QuadraticValidate() {
  Build();
  VerifyCellIterator();
  VerifyIndexContents();
  VerifyRangeIterators();
}

// Verifies that "expected" and "actual" have the same contents.  Note that
// duplicate values are allowed.
void ExpectEqual(vector<LabelledCell> expected, vector<LabelledCell> actual) {
  std::sort(expected.begin(), expected.end());
  std::sort(actual.begin(), actual.end());
  EXPECT_EQ(expected, actual);
}

// Verifies that S2CellIndex::CellIterator visits each (cell_id, label) pair
// exactly once.
void S2CellIndexTest::VerifyCellIterator() const {
  vector<LabelledCell> actual;
  for (S2CellIndex::CellIterator it(&index_); !it.done(); it.Next()) {
    actual.push_back(LabelledCell(it.cell_id(), it.label()));
  }
  ExpectEqual(contents_, std::move(actual));
}

void S2CellIndexTest::VerifyRangeIterators() const {
  // Test Finish(), which is not otherwise tested below.
  S2CellIndex::RangeIterator it(&index_);
  it.Begin();
  it.Finish();
  EXPECT_TRUE(it.done());

  // And also for non-empty ranges.
  S2CellIndex::NonEmptyRangeIterator non_empty(&index_);
  non_empty.Begin();
  non_empty.Finish();
  EXPECT_TRUE(non_empty.done());

  // Iterate through all the ranges in the index.  We simultaneously iterate
  // through the non-empty ranges and check that the correct ranges are found.
  S2CellId prev_start = S2CellId::None();
  S2CellId non_empty_prev_start = S2CellId::None();
  for (it.Begin(), non_empty.Begin(); !it.done(); it.Next()) {
    // Check that seeking in the current range takes us to this range.
    S2CellIndex::RangeIterator it2(&index_);
    S2CellId start = it.start_id();
    it2.Seek(it.start_id());
    EXPECT_EQ(start, it2.start_id());
    it2.Seek(it.limit_id().prev());
    EXPECT_EQ(start, it2.start_id());

    // And also for non-empty ranges.
    S2CellIndex::NonEmptyRangeIterator non_empty2(&index_);
    S2CellId non_empty_start = non_empty.start_id();
    non_empty2.Seek(it.start_id());
    EXPECT_EQ(non_empty_start, non_empty2.start_id());
    non_empty2.Seek(it.limit_id().prev());
    EXPECT_EQ(non_empty_start, non_empty2.start_id());

    // Test Prev() and Next().
    if (it2.Prev()) {
      EXPECT_EQ(prev_start, it2.start_id());
      it2.Next();
      EXPECT_EQ(start, it2.start_id());
    } else {
      EXPECT_EQ(start, it2.start_id());
      EXPECT_EQ(S2CellId::None(), prev_start);
    }

    // And also for non-empty ranges.
    if (non_empty2.Prev()) {
      EXPECT_EQ(non_empty_prev_start, non_empty2.start_id());
      non_empty2.Next();
      EXPECT_EQ(non_empty_start, non_empty2.start_id());
    } else {
      EXPECT_EQ(non_empty_start, non_empty2.start_id());
      EXPECT_EQ(S2CellId::None(), non_empty_prev_start);
    }

    // Keep the non-empty iterator synchronized with the regular one.
    if (!it.is_empty()) {
      EXPECT_EQ(it.start_id(), non_empty.start_id());
      EXPECT_EQ(it.limit_id(), non_empty.limit_id());
      EXPECT_FALSE(non_empty.done());
      non_empty_prev_start = non_empty_start;
      non_empty.Next();
    }
    prev_start = start;
  }
  // Verify that the NonEmptyRangeIterator is also finished.
  EXPECT_TRUE(non_empty.done());
}

// Verifies that RangeIterator and ContentsIterator can be used to determine
// the exact set of (s2cell_id, label) pairs that contain any leaf cell.
void S2CellIndexTest::VerifyIndexContents() const {
  // "min_cellid" is the first S2CellId that has not been validated yet.
  S2CellId min_cell_id = S2CellId::Begin(S2CellId::kMaxLevel);
  S2CellIndex::RangeIterator range(&index_);
  for (range.Begin(); !range.done(); range.Next()) {
    EXPECT_EQ(min_cell_id, range.start_id());
    EXPECT_LT(min_cell_id, range.limit_id());
    EXPECT_TRUE(range.limit_id().is_leaf());
    min_cell_id = range.limit_id();

    // Build a list of expected (cell_id, label) pairs for this range.
    vector<LabelledCell> expected;
    for (LabelledCell x : contents_) {
      if (x.cell_id.range_min() <= range.start_id() &&
          x.cell_id.range_max().next() >= range.limit_id()) {
        // The cell contains the entire range.
        expected.push_back(x);
      } else {
        // Verify that the cell does not intersect the range.
        EXPECT_FALSE(x.cell_id.range_min() <= range.limit_id().prev() &&
                     x.cell_id.range_max() >= range.start_id());
      }
    }
    vector<LabelledCell> actual;
    S2CellIndex::ContentsIterator contents(&index_);
    for (contents.StartUnion(range); !contents.done(); contents.Next()) {
      actual.push_back(LabelledCell(contents.cell_id(), contents.label()));
    }
    ExpectEqual(std::move(expected), std::move(actual));
  }
  EXPECT_EQ(S2CellId::End(S2CellId::kMaxLevel), min_cell_id);
}

TEST_F(S2CellIndexTest, Empty) {
  QuadraticValidate();
}

TEST_F(S2CellIndexTest, OneFaceCell) {
  Add("0/", 0);
  QuadraticValidate();
}

TEST_F(S2CellIndexTest, OneLeafCell) {
  Add("1/012301230123012301230123012301", 12);
  QuadraticValidate();
}

TEST_F(S2CellIndexTest, DuplicateValues) {
  Add("0/", 0);
  Add("0/", 0);
  Add("0/", 1);
  Add("0/", 17);
  QuadraticValidate();
}

TEST_F(S2CellIndexTest, DisjointCells) {
  Add("0/", 0);
  Add("3/", 0);
  QuadraticValidate();
}

TEST_F(S2CellIndexTest, NestedCells) {
  // Tests nested cells, including cases where several cells have the same
  // range_min() or range_max() and with randomly ordered labels.
  Add("1/", 3);
  Add("1/0", 15);
  Add("1/000", 9);
  Add("1/00000", 11);
  Add("1/012", 6);
  Add("1/01212", 5);
  Add("1/312", 17);
  Add("1/31200", 4);
  Add("1/3120000", 10);
  Add("1/333", 20);
  Add("1/333333", 18);
  Add("5/", 3);
  Add("5/3", 31);
  Add("5/3333", 27);
  QuadraticValidate();
}

// Creates a cell union from a small number of random cells at random levels.
S2CellUnion GetRandomCellUnion() {
  vector<S2CellId> ids;
  for (int j = 0; j < 10; ++j) {
    ids.push_back(S2Testing::GetRandomCellId());
  }
  return S2CellUnion(std::move(ids));
}

TEST_F(S2CellIndexTest, RandomCellUnions) {
  // Construct cell unions from random S2CellIds at random levels.  Note that
  // because the cell level is chosen uniformly, there is a very high
  // likelihood that the cell unions will overlap.
  for (int i = 0; i < 100; ++i) {
    Add(GetRandomCellUnion(), i);
  }
  QuadraticValidate();
}

// Given an S2CellId "target_str" in human-readable form, expects that the
// first leaf cell contained by this target will intersect the exact set of
// (cell_id, label) pairs given by "expected_strs".
void S2CellIndexTest::ExpectContents(
    const string& target_str, S2CellIndex::ContentsIterator* contents,
    const vector<pair<string, Label>>& expected_strs) const {
  S2CellIndex::RangeIterator range(&index_);
  range.Seek(S2CellId::FromDebugString(target_str).range_min());
  vector<LabelledCell> expected, actual;
  for (const auto& p : expected_strs) {
    expected.push_back(
        LabelledCell(S2CellId::FromDebugString(p.first), p.second));
  }
  for (contents->StartUnion(range); !contents->done(); contents->Next()) {
    actual.push_back(LabelledCell(contents->cell_id(), contents->label()));
  }
  ExpectEqual(expected, actual);
}

TEST_F(S2CellIndexTest, ContentsIteratorSuppressesDuplicates) {
  // Checks that ContentsIterator stops reporting values once it reaches a
  // node of the cell tree that was visited by the previous call to Begin().
  Add("2/1", 1);
  Add("2/1", 2);
  Add("2/10", 3);
  Add("2/100", 4);
  Add("2/102", 5);
  Add("2/1023", 6);
  Add("2/31", 7);
  Add("2/313", 8);
  Add("2/3132", 9);
  Add("3/1", 10);
  Add("3/12", 11);
  Add("3/13", 12);
  QuadraticValidate();

  S2CellIndex::ContentsIterator contents(&index_);
  ExpectContents("1/123", &contents, {});
  ExpectContents("2/100123", &contents,
                 {{"2/1", 1}, {"2/1", 2}, {"2/10", 3}, {"2/100", 4}});
  // Check that a second call with the same key yields no additional results.
  ExpectContents("2/100123", &contents, {});
  // Check that seeking to a different branch yields only the new values.
  ExpectContents("2/10232", &contents, {{"2/102", 5}, {"2/1023", 6}});
  // Seek to a node with a different root.
  ExpectContents("2/313", &contents, {{"2/31", 7}, {"2/313", 8}});
  // Seek to a descendant of the previous node.
  ExpectContents("2/3132333", &contents, {{"2/3132", 9}});
  // Seek to an ancestor of the previous node.
  ExpectContents("2/213", &contents, {});
  // A few more tests of incremental reporting.
  ExpectContents("3/1232", &contents, {{"3/1", 10}, {"3/12", 11}});
  ExpectContents("3/133210", &contents, {{"3/13", 12}});
  ExpectContents("3/133210", &contents, {});
  ExpectContents("5/0", &contents, {});

  // Now try moving backwards, which is expected to yield values that were
  // already reported above.
  ExpectContents("3/13221", &contents, {{"3/1", 10}, {"3/13", 12}});
  ExpectContents("2/31112", &contents, {{"2/31", 7}});
}

// Tests that VisitIntersectingCells() and GetIntersectingLabels() return
// correct results for the given target.
void S2CellIndexTest::TestIntersection(const S2CellUnion& target) {
  vector<LabelledCell> expected, actual;
  set<Label> expected_labels;
  for (S2CellIndex::CellIterator it(&index_); !it.done(); it.Next()) {
    if (target.Intersects(it.cell_id())) {
      expected.push_back(LabelledCell(it.cell_id(), it.label()));
      expected_labels.insert(it.label());
    }
  }
  index_.VisitIntersectingCells(
      target, [&actual](S2CellId cell_id, Label label) {
        actual.push_back({cell_id, label});
        return true;
      });
  ExpectEqual(expected, actual);
  vector<Label> actual_labels = index_.GetIntersectingLabels(target);
  std::sort(actual_labels.begin(), actual_labels.end());
  EXPECT_EQ(vector<Label>(expected_labels.begin(), expected_labels.end()),
            actual_labels);
}

S2CellUnion MakeCellUnion(const vector<string>& strs) {
  vector<S2CellId> ids;
  for (const auto& str : strs) {
    ids.push_back(S2CellId::FromDebugString(str));
  }
  return S2CellUnion(std::move(ids));
}

TEST_F(S2CellIndexTest, IntersectionOptimization) {
  // Tests various corner cases for the binary search optimization in
  // VisitIntersectingCells.

  Add("1/001", 1);
  Add("1/333", 2);
  Add("2/00", 3);
  Add("2/0232", 4);
  Build();
  TestIntersection(MakeCellUnion({"1/010", "1/3"}));
  TestIntersection(MakeCellUnion({"2/010", "2/011", "2/02"}));
}

TEST_F(S2CellIndexTest, IntersectionRandomUnions) {
  // Construct cell unions from random S2CellIds at random levels.  Note that
  // because the cell level is chosen uniformly, there is a very high
  // likelihood that the cell unions will overlap.
  for (int i = 0; i < 100; ++i) {
    Add(GetRandomCellUnion(), i);
  }
  Build();
  // Now repeatedly query a cell union constructed in the same way.
  for (int i = 0; i < 200; ++i) {
    TestIntersection(GetRandomCellUnion());
  }
}

TEST_F(S2CellIndexTest, IntersectionSemiRandomUnions) {
  // This test also uses random S2CellUnions, but the unions are specially
  // constructed so that interesting cases are more likely to arise.
  for (int iter = 0; iter < 200; ++iter) {
    index_.Clear();
    S2CellId id = S2CellId::FromDebugString("1/0123012301230123");
    vector<S2CellId> target;
    for (int i = 0; i < 100; ++i) {
      if (S2Testing::rnd.OneIn(10)) Add(id, i);
      if (S2Testing::rnd.OneIn(4)) target.push_back(id);
      if (S2Testing::rnd.OneIn(2)) id = id.next_wrap();
      if (S2Testing::rnd.OneIn(6) && !id.is_face()) id = id.parent();
      if (S2Testing::rnd.OneIn(6) && !id.is_leaf()) id = id.child_begin();
    }
    Build();
    TestIntersection(S2CellUnion(std::move(target)));
  }
}

}  // namespace
