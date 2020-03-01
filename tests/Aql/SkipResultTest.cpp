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
  auto testee = SkipResult::fromVelocyPack(builder.slice());
  EXPECT_EQ(testee.nothingSkipped(), original.nothingSkipped());
  EXPECT_EQ(testee.getSkipCount(), original.getSkipCount());
}

TEST_F(SkipResultTest, serialize_deserialize_with_count) {
  SkipResult original{};
  original.didSkip(6);
  VPackBuilder builder;
  original.toVelocyPack(builder);
  auto testee = SkipResult::fromVelocyPack(builder.slice());
  EXPECT_EQ(testee.nothingSkipped(), original.nothingSkipped());
  EXPECT_EQ(testee.getSkipCount(), original.getSkipCount());
}

TEST_F(SkipResult, can_be_added) {
  SkipResult a{};
  a.didSkip(6);
  SkipResult b{};
  b.didSkip(7);
  a += b;
  EXPECT_EQ(a.getSkipCount(), 13);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
