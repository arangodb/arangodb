////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/Match/PatternTypes.h"

#include <string>
#include <vector>

using namespace arangodb::aql::match;

TEST(PatternTypesTest, dataSourceCollectionAndBindParameter) {
  auto collection = DataSource::collection("vc");
  EXPECT_EQ(DataSource::Kind::kCollection, collection.kind());
  EXPECT_EQ("vc", collection.name());

  auto bind = DataSource::bindParameter("@@vc");
  EXPECT_EQ(DataSource::Kind::kBindParameter, bind.kind());
  EXPECT_EQ("@@vc", bind.name());
}

TEST(PatternTypesTest, pathRangeDefaultFixedOne) {
  auto range = PathRange::defaultFixedOne();
  EXPECT_TRUE(range.isDefaultFixedOne());
  EXPECT_TRUE(range.isFixedOne());
  EXPECT_TRUE(range.isFixed());
  EXPECT_EQ(1U, range.minDepth());
  ASSERT_TRUE(range.hasMaxDepth());
  EXPECT_EQ(1U, range.maxDepth());
}

TEST(PatternTypesTest, pathRangeExplicitFixedOneIsNotDefault) {
  auto range = PathRange::bounded(1, 1);
  EXPECT_FALSE(range.isDefaultFixedOne());
  EXPECT_TRUE(range.isFixedOne());
  EXPECT_TRUE(range.isFixed());
  EXPECT_EQ(PathRange::Kind::kBounded, range.kind());
}

TEST(PatternTypesTest, pathRangeBoundedAndFixed) {
  auto range = PathRange::bounded(2, 2);
  EXPECT_FALSE(range.isDefaultFixedOne());
  EXPECT_FALSE(range.isFixedOne());
  EXPECT_TRUE(range.isFixed());
  EXPECT_EQ(2U, range.minDepth());
  EXPECT_EQ(2U, range.maxDepth());

  auto bounded = PathRange::bounded(1, 3);
  EXPECT_FALSE(bounded.isFixed());
  EXPECT_FALSE(bounded.isFixedOne());
  EXPECT_EQ(1U, bounded.minDepth());
  EXPECT_EQ(3U, bounded.maxDepth());
}

TEST(PatternTypesTest, pathRangeUnboundedMin) {
  auto range = PathRange::unboundedMin(2);
  EXPECT_EQ(PathRange::Kind::kUnboundedMin, range.kind());
  EXPECT_FALSE(range.isDefaultFixedOne());
  EXPECT_FALSE(range.isFixed());
  EXPECT_FALSE(range.isFixedOne());
  EXPECT_EQ(2U, range.minDepth());
  EXPECT_FALSE(range.hasMaxDepth());
}

TEST(PatternTypesTest, projectionItemKeepPath) {
  auto nested = ProjectionItem::keepPath({"profile", "name"});
  EXPECT_EQ(ProjectionItem::Kind::kKeepAttribute, nested.kind);
  EXPECT_TRUE(nested.isKeep());
  EXPECT_FALSE(nested.isAlias());
  EXPECT_TRUE(nested.name.empty());
  EXPECT_EQ((std::vector<std::string>{"profile", "name"}), nested.path);
  EXPECT_EQ("profile", nested.topLevelKey());

  auto single = ProjectionItem::keepPath({"status"});
  EXPECT_EQ("status", single.name);
  EXPECT_EQ("status", single.topLevelKey());
}

TEST(PatternTypesTest, projectionItemKeepLiteralAndAlias) {
  auto literal = ProjectionItem::keepLiteral("profile.name");
  EXPECT_EQ(ProjectionItem::Kind::kKeepLiteral, literal.kind);
  EXPECT_TRUE(literal.isKeep());
  EXPECT_EQ("profile.name", literal.name);
  EXPECT_EQ((std::vector<std::string>{"profile.name"}), literal.path);
  EXPECT_EQ("profile.name", literal.topLevelKey());

  ExpressionRef expr{};
  auto alias = ProjectionItem::alias("idx", expr);
  EXPECT_EQ(ProjectionItem::Kind::kAlias, alias.kind);
  EXPECT_TRUE(alias.isAlias());
  EXPECT_FALSE(alias.isKeep());
  EXPECT_EQ("idx", alias.name);
  EXPECT_TRUE(alias.path.empty());
  EXPECT_EQ("idx", alias.topLevelKey());
}
