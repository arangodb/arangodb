////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "Aql/Projections.h"

using namespace arangodb::aql;

TEST(ProjectionsTest, buildEmpty) {
  Projections p;

  ASSERT_EQ(0, p.size());
  ASSERT_TRUE(p.empty());
  ASSERT_FALSE(p.isSingle("a"));
  ASSERT_FALSE(p.isSingle("_key"));
}

TEST(ProjectionsTest, buildSingleKey) {
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
    AttributeNamePath("_key"),
  };
  Projections p(std::move(attributes));

  ASSERT_EQ(1, p.size());
  ASSERT_FALSE(p.empty());
  ASSERT_EQ(AttributeNamePath("_key"), p[0].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::KeyAttribute, p[0].type);
  ASSERT_FALSE(p.isSingle("a"));
  ASSERT_TRUE(p.isSingle("_key"));
}

TEST(ProjectionsTest, buildSingleId) {
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
    AttributeNamePath("_id"),
  };
  Projections p(std::move(attributes));

  ASSERT_EQ(1, p.size());
  ASSERT_FALSE(p.empty());
  ASSERT_EQ(AttributeNamePath("_id"), p[0].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::IdAttribute, p[0].type);
  ASSERT_FALSE(p.isSingle("a"));
  ASSERT_TRUE(p.isSingle("_id"));
}

TEST(ProjectionsTest, buildSingleFrom) {
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
    AttributeNamePath("_from"),
  };
  Projections p(std::move(attributes));

  ASSERT_EQ(1, p.size());
  ASSERT_FALSE(p.empty());
  ASSERT_EQ(AttributeNamePath("_from"), p[0].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::FromAttribute, p[0].type);
  ASSERT_FALSE(p.isSingle("a"));
  ASSERT_TRUE(p.isSingle("_from"));
}

TEST(ProjectionsTest, buildSingleTo) {
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
    AttributeNamePath("_to"),
  };
  Projections p(std::move(attributes));

  ASSERT_EQ(1, p.size());
  ASSERT_FALSE(p.empty());
  ASSERT_EQ(AttributeNamePath("_to"), p[0].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::ToAttribute, p[0].type);
  ASSERT_FALSE(p.isSingle("a"));
  ASSERT_TRUE(p.isSingle("_to"));
}

TEST(ProjectionsTest, buildSingleOther) {
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
    AttributeNamePath("piff"),
  };
  Projections p(std::move(attributes));

  ASSERT_EQ(1, p.size());
  ASSERT_FALSE(p.empty());
  ASSERT_EQ(AttributeNamePath("piff"), p[0].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[0].type);
  ASSERT_FALSE(p.isSingle("a"));
  ASSERT_TRUE(p.isSingle("piff"));
}

TEST(ProjectionsTest, buildMulti) {
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
    AttributeNamePath("a"),
    AttributeNamePath("b"),
    AttributeNamePath("c"),
  };
  Projections p(std::move(attributes));

  ASSERT_EQ(3, p.size());
  ASSERT_FALSE(p.empty());
  ASSERT_EQ(AttributeNamePath("a"), p[0].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[0].type);
  ASSERT_EQ(AttributeNamePath("b"), p[1].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[1].type);
  ASSERT_EQ(AttributeNamePath("c"), p[2].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[2].type);
  ASSERT_FALSE(p.isSingle("a"));
  ASSERT_FALSE(p.isSingle("b"));
  ASSERT_FALSE(p.isSingle("c"));
  ASSERT_FALSE(p.isSingle("_key"));
}

TEST(ProjectionsTest, buildReverse) {
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
    AttributeNamePath("c"),
    AttributeNamePath("b"),
    AttributeNamePath("a"),
  };
  Projections p(std::move(attributes));

  ASSERT_EQ(3, p.size());
  ASSERT_FALSE(p.empty());
  ASSERT_EQ(AttributeNamePath("a"), p[0].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[0].type);
  ASSERT_EQ(AttributeNamePath("b"), p[1].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[1].type);
  ASSERT_EQ(AttributeNamePath("c"), p[2].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[2].type);
  ASSERT_FALSE(p.isSingle("a"));
  ASSERT_FALSE(p.isSingle("b"));
  ASSERT_FALSE(p.isSingle("c"));
  ASSERT_FALSE(p.isSingle("_key"));
}

TEST(ProjectionsTest, buildWithSystem) {
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
    AttributeNamePath("a"),
    AttributeNamePath("_key"),
    AttributeNamePath("_id"),
  };
  Projections p(std::move(attributes));

  ASSERT_EQ(3, p.size());
  ASSERT_FALSE(p.empty());
  ASSERT_EQ(AttributeNamePath("_id"), p[0].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::IdAttribute, p[0].type);
  ASSERT_EQ(AttributeNamePath("_key"), p[1].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::KeyAttribute, p[1].type);
  ASSERT_EQ(AttributeNamePath("a"), p[2].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[2].type);
  ASSERT_FALSE(p.isSingle("a"));
  ASSERT_FALSE(p.isSingle("_key"));
  ASSERT_FALSE(p.isSingle("_id"));
}

TEST(ProjectionsTest, buildNested1) {
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
    AttributeNamePath(std::vector<std::string>({"a", "b"})),
    AttributeNamePath("_key"),
    AttributeNamePath(std::vector<std::string>({"a", "z", "A"})),
  };
  Projections p(std::move(attributes));

  ASSERT_EQ(2, p.size());
  ASSERT_FALSE(p.empty());
  ASSERT_EQ(AttributeNamePath("_key"), p[0].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::KeyAttribute, p[0].type);
  ASSERT_EQ(AttributeNamePath("a"), p[1].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[1].type);
  ASSERT_FALSE(p.isSingle("a"));
  ASSERT_FALSE(p.isSingle("_key"));
  ASSERT_FALSE(p.isSingle("z"));
}

TEST(ProjectionsTest, buildNested2) {
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
    AttributeNamePath(std::vector<std::string>({"b", "b"})),
    AttributeNamePath("_key"),
    AttributeNamePath(std::vector<std::string>({"a", "z", "A"})),
    AttributeNamePath("A"),
  };
  Projections p(std::move(attributes));

  ASSERT_EQ(4, p.size());
  ASSERT_FALSE(p.empty());
  ASSERT_EQ(AttributeNamePath("A"), p[0].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[0].type);
  ASSERT_EQ(AttributeNamePath("_key"), p[1].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::KeyAttribute, p[1].type);
  ASSERT_EQ(AttributeNamePath(std::vector<std::string>({{"a"}, {"z"}, {"A"}})), p[2].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::MultiAttribute, p[2].type);
  ASSERT_EQ(AttributeNamePath(std::vector<std::string>({{"b"}, {"b"}})), p[3].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::MultiAttribute, p[3].type);
}

TEST(ProjectionsTest, buildOverlapping1) {
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
    AttributeNamePath("a"),
    AttributeNamePath(std::vector<std::string>({"a", "b", "c"})),
  };
  Projections p(std::move(attributes));

  ASSERT_EQ(1, p.size());
  ASSERT_EQ(AttributeNamePath("a"), p[0].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[0].type);
}

TEST(ProjectionsTest, buildOverlapping2) {
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
    AttributeNamePath(std::vector<std::string>({"a", "b", "c"})),
    AttributeNamePath("a"),
  };
  Projections p(std::move(attributes));

  ASSERT_EQ(1, p.size());
  ASSERT_EQ(AttributeNamePath("a"), p[0].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[0].type);
}

TEST(ProjectionsTest, buildOverlapping3) {
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
    AttributeNamePath(std::vector<std::string>({"a", "b", "c"})),
    AttributeNamePath(std::vector<std::string>({"a", "b"})),
  };
  Projections p(std::move(attributes));

  ASSERT_EQ(1, p.size());
  ASSERT_EQ(AttributeNamePath(std::vector<std::string>({{"a"}, {"b"}})), p[0].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::MultiAttribute, p[0].type);
}

TEST(ProjectionsTest, buildOverlapping4) {
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
    AttributeNamePath("m"),
    AttributeNamePath(std::vector<std::string>({"a", "b", "c"})),
    AttributeNamePath(std::vector<std::string>({"a", "b", "c"})),
    AttributeNamePath("b"),
  };
  Projections p(std::move(attributes));

  ASSERT_EQ(3, p.size());
  ASSERT_EQ(AttributeNamePath(std::vector<std::string>({{"a"}, {"b"}, {"c"}})), p[0].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::MultiAttribute, p[0].type);
  ASSERT_EQ(AttributeNamePath("b"), p[1].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[1].type);
  ASSERT_EQ(AttributeNamePath("m"), p[2].path);
  ASSERT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[2].type);
}
