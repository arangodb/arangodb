////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/SkipResult.h"
#include "Cluster/ResultT.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

class SkipResultTest : public ::testing::Test {
 protected:
  SkipResultTest() {}
};

TEST_F(SkipResultTest, defaults_to_0_skip) {
  SkipResult testee{};
  EXPECT_EQ(testee.getSkipCount(), 0);
}

TEST_F(SkipResultTest, counts_skip) {
  SkipResult testee{};
  testee.didSkip(5);
  EXPECT_EQ(testee.getSkipCount(), 5);
}

TEST_F(SkipResultTest, accumulates_skips) {
  SkipResult testee{};
  testee.didSkip(3);
  testee.didSkip(6);
  testee.didSkip(8);
  EXPECT_EQ(testee.getSkipCount(), 17);
}

TEST_F(SkipResultTest, is_copyable) {
  SkipResult original{};
  original.didSkip(6);
  SkipResult testee{original};

  EXPECT_EQ(testee.getSkipCount(), original.getSkipCount());

  original.didSkip(7);
  EXPECT_NE(testee.getSkipCount(), original.getSkipCount());
}

TEST_F(SkipResultTest, can_report_if_we_skip) {
  SkipResult testee{};
  EXPECT_TRUE(testee.nothingSkipped());
  testee.didSkip(3);
  EXPECT_FALSE(testee.nothingSkipped());
  testee.didSkip(6);
  EXPECT_FALSE(testee.nothingSkipped());
}

TEST_F(SkipResultTest, serialize_deserialize_empty) {
  SkipResult original{};
  VPackBuilder builder;
  original.toVelocyPack(builder);
  auto maybeTestee = SkipResult::fromVelocyPack(builder.slice());
  ASSERT_FALSE(maybeTestee.fail());
  auto testee = maybeTestee.get();
  EXPECT_EQ(testee.nothingSkipped(), original.nothingSkipped());
  EXPECT_EQ(testee.getSkipCount(), original.getSkipCount());
  EXPECT_EQ(testee, original);
}

TEST_F(SkipResultTest, serialize_deserialize_with_count) {
  SkipResult original{};
  original.didSkip(6);
  VPackBuilder builder;
  original.toVelocyPack(builder);
  auto maybeTestee = SkipResult::fromVelocyPack(builder.slice());
  ASSERT_FALSE(maybeTestee.fail());
  auto testee = maybeTestee.get();
  EXPECT_EQ(testee.nothingSkipped(), original.nothingSkipped());
  EXPECT_EQ(testee.getSkipCount(), original.getSkipCount());
  EXPECT_EQ(testee, original);
}

TEST_F(SkipResultTest, can_be_added) {
  SkipResult a{};
  a.didSkip(6);
  SkipResult b{};
  b.didSkip(7);
  a += b;
  EXPECT_EQ(a.getSkipCount(), 13);
}

TEST_F(SkipResultTest, can_add_a_subquery_depth) {
  SkipResult a{};
  a.didSkip(5);
  EXPECT_EQ(a.getSkipCount(), 5);
  a.incrementSubquery();
  EXPECT_EQ(a.getSkipCount(), 0);
  a.didSkip(7);
  EXPECT_EQ(a.getSkipCount(), 7);
  a.decrementSubquery();
  EXPECT_EQ(a.getSkipCount(), 5);
}

TEST_F(SkipResultTest, nothing_skip_on_subquery) {
  SkipResult a{};
  EXPECT_TRUE(a.nothingSkipped());
  a.didSkip(6);
  EXPECT_FALSE(a.nothingSkipped());
  a.incrementSubquery();
  EXPECT_EQ(a.getSkipCount(), 0);
  EXPECT_FALSE(a.nothingSkipped());
}

TEST_F(SkipResultTest, serialize_deserialize_with_a_subquery) {
  SkipResult original{};
  original.didSkip(6);
  original.incrementSubquery();
  original.didSkip(2);

  VPackBuilder builder;
  original.toVelocyPack(builder);
  auto maybeTestee = SkipResult::fromVelocyPack(builder.slice());
  ASSERT_FALSE(maybeTestee.fail());
  auto testee = maybeTestee.get();
  // Use built_in eq
  EXPECT_EQ(testee, original);
  // Manual test
  EXPECT_EQ(testee.nothingSkipped(), original.nothingSkipped());
  EXPECT_EQ(testee.getSkipCount(), original.getSkipCount());
  EXPECT_EQ(testee.subqueryDepth(), original.subqueryDepth());
  original.decrementSubquery();
  testee.decrementSubquery();
  EXPECT_EQ(testee.nothingSkipped(), original.nothingSkipped());
  EXPECT_EQ(testee.getSkipCount(), original.getSkipCount());
  EXPECT_EQ(testee.subqueryDepth(), original.subqueryDepth());
}

TEST_F(SkipResultTest, equality) {
  auto buildTestSet = []() -> std::vector<SkipResult> {
    SkipResult empty{};
    SkipResult skip1{};
    skip1.didSkip(6);

    SkipResult skip2{};
    skip2.didSkip(8);

    SkipResult subQuery1{};
    subQuery1.incrementSubquery();
    subQuery1.didSkip(4);

    SkipResult subQuery2{};
    subQuery2.didSkip(8);
    subQuery2.incrementSubquery();
    subQuery2.didSkip(4);

    SkipResult subQuery3{};
    subQuery3.didSkip(8);
    subQuery3.incrementSubquery();
    return {empty, skip1, skip2, subQuery1, subQuery2, subQuery3};
  };

  // We create two identical sets with different entries
  auto set1 = buildTestSet();
  auto set2 = buildTestSet();
  for (size_t i = 0; i < set1.size(); ++i) {
    for (size_t j = 0; j < set2.size(); ++j) {
      // Addresses are different
      EXPECT_NE(&set1.at(i), &set2.at(j));
      // Identical index => Equal object
      if (i == j) {
        EXPECT_EQ(set1.at(i), set2.at(j));
      } else {
        EXPECT_NE(set1.at(i), set2.at(j));
      }
    }
  }
}

TEST_F(SkipResultTest, merge_with_toplevel) {
  SkipResult a{};
  a.didSkip(12);
  a.incrementSubquery();
  a.didSkip(8);

  SkipResult b{};
  b.didSkip(9);
  b.incrementSubquery();
  b.didSkip(2);

  a.merge(b, false);

  SkipResult expected{};
  expected.didSkip(12);
  expected.didSkip(9);
  expected.incrementSubquery();
  expected.didSkip(8);
  expected.didSkip(2);
  EXPECT_EQ(a, expected);
}

TEST_F(SkipResultTest, merge_without_toplevel) {
  SkipResult a{};
  a.didSkip(12);
  a.incrementSubquery();
  a.didSkip(8);

  SkipResult b{};
  b.didSkip(9);
  b.incrementSubquery();
  b.didSkip(2);

  a.merge(b, true);

  SkipResult expected{};
  expected.didSkip(12);
  expected.didSkip(9);
  expected.incrementSubquery();
  expected.didSkip(8);
  EXPECT_EQ(a, expected);
}

TEST_F(SkipResultTest, reset) {
  SkipResult a{};
  a.didSkip(12);
  a.incrementSubquery();
  a.didSkip(8);

  EXPECT_EQ(a.getSkipCount(), 8);
  EXPECT_EQ(a.subqueryDepth(), 2);
  EXPECT_FALSE(a.nothingSkipped());
  a.reset();

  EXPECT_EQ(a.getSkipCount(), 0);
  EXPECT_EQ(a.subqueryDepth(), 2);
  EXPECT_TRUE(a.nothingSkipped());

  a.decrementSubquery();
  EXPECT_EQ(a.getSkipCount(), 0);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
