////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"

#include "Aql/Projections.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/LogicalCollection.h"
#include "Basics/GlobalResourceMonitor.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;

namespace {
auto createAttributeNamePath = [](std::vector<std::string>&& vec,
                                  arangodb::ResourceMonitor& resMonitor) {
  return AttributeNamePath(std::move(vec), resMonitor);
};
}

TEST(ProjectionsTest, buildEmpty) {
  Projections p;

  EXPECT_EQ(0, p.size());
  EXPECT_TRUE(p.empty());
  EXPECT_FALSE(p.isSingle("a"));
  EXPECT_FALSE(p.isSingle("_key"));
}

TEST(ProjectionsTest, buildSingleKey) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      AttributeNamePath("_key", resMonitor),
  };
  Projections p(std::move(attributes));

  EXPECT_EQ(1, p.size());
  EXPECT_FALSE(p.empty());
  EXPECT_EQ(AttributeNamePath("_key", resMonitor), p[0].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::KeyAttribute, p[0].type);
  EXPECT_FALSE(p.isSingle("a"));
  EXPECT_TRUE(p.isSingle("_key"));
}

TEST(ProjectionsTest, buildSingleId) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      AttributeNamePath("_id", resMonitor),
  };
  Projections p(std::move(attributes));

  EXPECT_EQ(1, p.size());
  EXPECT_FALSE(p.empty());
  EXPECT_EQ(AttributeNamePath("_id", resMonitor), p[0].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::IdAttribute, p[0].type);
  EXPECT_FALSE(p.isSingle("a"));
  EXPECT_TRUE(p.isSingle("_id"));
}

TEST(ProjectionsTest, buildSingleFrom) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      AttributeNamePath("_from", resMonitor),
  };
  Projections p(std::move(attributes));

  EXPECT_EQ(1, p.size());
  EXPECT_FALSE(p.empty());
  EXPECT_EQ(AttributeNamePath("_from", resMonitor), p[0].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::FromAttribute, p[0].type);
  EXPECT_FALSE(p.isSingle("a"));
  EXPECT_TRUE(p.isSingle("_from"));
}

TEST(ProjectionsTest, buildSingleTo) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      AttributeNamePath("_to", resMonitor),
  };
  Projections p(std::move(attributes));

  EXPECT_EQ(1, p.size());
  EXPECT_FALSE(p.empty());
  EXPECT_EQ(AttributeNamePath("_to", resMonitor), p[0].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::ToAttribute, p[0].type);
  EXPECT_FALSE(p.isSingle("a"));
  EXPECT_TRUE(p.isSingle("_to"));
}

TEST(ProjectionsTest, buildSingleOther) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      AttributeNamePath("piff", resMonitor),
  };
  Projections p(std::move(attributes));

  EXPECT_EQ(1, p.size());
  EXPECT_FALSE(p.empty());
  EXPECT_EQ(AttributeNamePath("piff", resMonitor), p[0].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[0].type);
  EXPECT_FALSE(p.isSingle("a"));
  EXPECT_TRUE(p.isSingle("piff"));
}

TEST(ProjectionsTest, buildMulti) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      AttributeNamePath("a", resMonitor),
      AttributeNamePath("b", resMonitor),
      AttributeNamePath("c", resMonitor),
  };
  Projections p(std::move(attributes));

  EXPECT_EQ(3, p.size());
  EXPECT_FALSE(p.empty());
  EXPECT_EQ(AttributeNamePath("a", resMonitor), p[0].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[0].type);
  EXPECT_EQ(AttributeNamePath("b", resMonitor), p[1].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[1].type);
  EXPECT_EQ(AttributeNamePath("c", resMonitor), p[2].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[2].type);
  EXPECT_FALSE(p.isSingle("a"));
  EXPECT_FALSE(p.isSingle("b"));
  EXPECT_FALSE(p.isSingle("c"));
  EXPECT_FALSE(p.isSingle("_key"));
}

TEST(ProjectionsTest, buildReverse) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      AttributeNamePath("c", resMonitor),
      AttributeNamePath("b", resMonitor),
      AttributeNamePath("a", resMonitor),
  };
  Projections p(std::move(attributes));

  EXPECT_EQ(3, p.size());
  EXPECT_FALSE(p.empty());
  EXPECT_EQ(AttributeNamePath("a", resMonitor), p[0].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[0].type);
  EXPECT_EQ(AttributeNamePath("b", resMonitor), p[1].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[1].type);
  EXPECT_EQ(AttributeNamePath("c", resMonitor), p[2].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[2].type);
  EXPECT_FALSE(p.isSingle("a"));
  EXPECT_FALSE(p.isSingle("b"));
  EXPECT_FALSE(p.isSingle("c"));
  EXPECT_FALSE(p.isSingle("_key"));
}

TEST(ProjectionsTest, buildWithSystem) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      AttributeNamePath("a", resMonitor),
      AttributeNamePath("_key", resMonitor),
      AttributeNamePath("_id", resMonitor),
  };
  Projections p(std::move(attributes));

  EXPECT_EQ(3, p.size());
  EXPECT_FALSE(p.empty());
  EXPECT_EQ(AttributeNamePath("_id", resMonitor), p[0].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::IdAttribute, p[0].type);
  EXPECT_EQ(AttributeNamePath("_key", resMonitor), p[1].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::KeyAttribute, p[1].type);
  EXPECT_EQ(AttributeNamePath("a", resMonitor), p[2].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[2].type);
  EXPECT_FALSE(p.isSingle("a"));
  EXPECT_FALSE(p.isSingle("_key"));
  EXPECT_FALSE(p.isSingle("_id"));
}

TEST(ProjectionsTest, buildNested1) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      createAttributeNamePath({"a", "b"}, resMonitor),
      AttributeNamePath("_key", resMonitor),
      createAttributeNamePath({"a", "z", "A"}, resMonitor),
  };
  Projections p(std::move(attributes));

  EXPECT_EQ(3, p.size());
  EXPECT_FALSE(p.empty());
  EXPECT_EQ(AttributeNamePath("_key", resMonitor), p[0].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::KeyAttribute, p[0].type);
  EXPECT_EQ(createAttributeNamePath({{"a"}, {"b"}}, resMonitor), p[1].path);
  EXPECT_EQ(createAttributeNamePath({{"a"}, {"z"}, {"A"}}, resMonitor),
            p[2].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::MultiAttribute, p[1].type);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::MultiAttribute, p[2].type);
  EXPECT_FALSE(p.isSingle("a"));
  EXPECT_FALSE(p.isSingle("b"));
  EXPECT_FALSE(p.isSingle("z"));
  EXPECT_FALSE(p.isSingle("_key"));
}

TEST(ProjectionsTest, buildNested2) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      createAttributeNamePath({"b", "b"}, resMonitor),
      AttributeNamePath("_key", resMonitor),
      createAttributeNamePath({"a", "z", "A"}, resMonitor),
      AttributeNamePath("A", resMonitor),
  };
  Projections p(std::move(attributes));

  EXPECT_EQ(4, p.size());
  EXPECT_FALSE(p.empty());
  EXPECT_EQ(AttributeNamePath("A", resMonitor), p[0].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[0].type);
  EXPECT_EQ(AttributeNamePath("_key", resMonitor), p[1].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::KeyAttribute, p[1].type);
  EXPECT_EQ(createAttributeNamePath({{"a"}, {"z"}, {"A"}}, resMonitor),
            p[2].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::MultiAttribute, p[2].type);
  EXPECT_EQ(createAttributeNamePath({{"b"}, {"b"}}, resMonitor), p[3].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::MultiAttribute, p[3].type);
}

TEST(ProjectionsTest, buildOverlapping1) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      AttributeNamePath("a", resMonitor),
      createAttributeNamePath({"a", "b", "c"}, resMonitor),
  };
  Projections p(std::move(attributes));

  EXPECT_EQ(1, p.size());
  EXPECT_EQ(AttributeNamePath("a", resMonitor), p[0].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[0].type);
}

TEST(ProjectionsTest, buildOverlapping2) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      createAttributeNamePath({"a", "b", "c"}, resMonitor),
      AttributeNamePath("a", resMonitor),
  };
  Projections p(std::move(attributes));

  EXPECT_EQ(1, p.size());
  EXPECT_EQ(AttributeNamePath("a", resMonitor), p[0].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[0].type);
}

TEST(ProjectionsTest, buildOverlapping3) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      createAttributeNamePath({"a", "b", "c"}, resMonitor),
      createAttributeNamePath({"a", "b"}, resMonitor),
  };
  Projections p(std::move(attributes));

  EXPECT_EQ(1, p.size());
  EXPECT_EQ(createAttributeNamePath({{"a"}, {"b"}}, resMonitor), p[0].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::MultiAttribute, p[0].type);
}

TEST(ProjectionsTest, buildOverlapping4) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      AttributeNamePath("m", resMonitor),
      createAttributeNamePath({"a", "b", "c"}, resMonitor),
      createAttributeNamePath({"a", "b", "c"}, resMonitor),
      AttributeNamePath("b", resMonitor),
  };
  Projections p(std::move(attributes));

  EXPECT_EQ(3, p.size());
  EXPECT_EQ(createAttributeNamePath({{"a"}, {"b"}, {"c"}}, resMonitor),
            p[0].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::MultiAttribute, p[0].type);
  EXPECT_EQ(AttributeNamePath("b", resMonitor), p[1].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[1].type);
  EXPECT_EQ(AttributeNamePath("m", resMonitor), p[2].path);
  EXPECT_EQ(arangodb::aql::AttributeNamePath::Type::SingleAttribute, p[2].type);
}

TEST(ProjectionsTest, buildOverlapping5) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      AttributeNamePath("a", resMonitor),
      createAttributeNamePath({"a", "b"}, resMonitor),
      createAttributeNamePath({"a", "c"}, resMonitor),
  };
  Projections p(std::move(attributes));

  EXPECT_EQ(1, p.size());
  EXPECT_EQ(AttributeNamePath("a", resMonitor), p[0].path);
}

TEST(ProjectionsTest, buildOverlapping6) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      createAttributeNamePath({"a", "c"}, resMonitor),
      createAttributeNamePath({"a", "b"}, resMonitor),
  };
  Projections p(std::move(attributes));

  EXPECT_EQ(2, p.size());
  EXPECT_EQ(createAttributeNamePath({{"a"}, {"b"}}, resMonitor), p[0].path);
  EXPECT_EQ(createAttributeNamePath({{"a"}, {"c"}}, resMonitor), p[1].path);
}

TEST(ProjectionsTest, erase) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      createAttributeNamePath({"a"}, resMonitor),
      createAttributeNamePath({"b"}, resMonitor),
      createAttributeNamePath({"c"}, resMonitor),
      createAttributeNamePath({"d"}, resMonitor),
  };
  Projections p(std::move(attributes));

  EXPECT_EQ(4, p.size());
  p.erase([](Projections::Projection& p) {
    return (p.path.get().size() == 1 && p.path.get()[0] == "b");
  });

  EXPECT_EQ(3, p.size());
  EXPECT_EQ(createAttributeNamePath({{"a"}}, resMonitor), p[0].path);
  EXPECT_EQ(createAttributeNamePath({{"c"}}, resMonitor), p[1].path);
  EXPECT_EQ(createAttributeNamePath({{"d"}}, resMonitor), p[2].path);

  p.erase([](Projections::Projection& p) {
    return (p.path.get().size() == 1 && p.path.get()[0] == "d");
  });

  EXPECT_EQ(2, p.size());
  EXPECT_EQ(createAttributeNamePath({{"a"}}, resMonitor), p[0].path);
  EXPECT_EQ(createAttributeNamePath({{"c"}}, resMonitor), p[1].path);

  p.erase([](Projections::Projection& p) {
    return (p.path.get().size() == 1 && p.path.get()[0] == "a");
  });

  EXPECT_EQ(1, p.size());
  EXPECT_EQ(createAttributeNamePath({{"c"}}, resMonitor), p[0].path);

  p.erase([](Projections::Projection& p) {
    return (p.path.get().size() == 1 && p.path.get()[0] == "c");
  });

  EXPECT_EQ(0, p.size());

  p.erase([](Projections::Projection& p) {
    return (p.path.get().size() == 1 && p.path.get()[0] == "c");
  });

  EXPECT_EQ(0, p.size());
}

TEST(ProjectionsTest, toVelocyPackFromDocumentSimple1) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      createAttributeNamePath({"a"}, resMonitor),
      createAttributeNamePath({"b"}, resMonitor),
      createAttributeNamePath({"c"}, resMonitor),
  };
  Projections p(std::move(attributes));

  velocypack::Builder out;

  auto prepareResult = [&p](std::string_view json, velocypack::Builder& out) {
    auto document = velocypack::Parser::fromJson(json.data(), json.size());
    out.clear();
    out.openObject();
    p.toVelocyPackFromDocument(out, document->slice(), nullptr);
    out.close();
    return out.slice();
  };

  {
    VPackSlice s = prepareResult("{}", out);
    EXPECT_EQ(3, s.length());
    EXPECT_TRUE(s.hasKey("a"));
    EXPECT_TRUE(s.get("a").isNull());
    EXPECT_TRUE(s.hasKey("b"));
    EXPECT_TRUE(s.get("b").isNull());
    EXPECT_TRUE(s.hasKey("c"));
    EXPECT_TRUE(s.get("c").isNull());
  }

  {
    VPackSlice s = prepareResult("{\"z\":1}", out);
    EXPECT_EQ(3, s.length());
    EXPECT_TRUE(s.hasKey("a"));
    EXPECT_TRUE(s.get("a").isNull());
    EXPECT_TRUE(s.hasKey("b"));
    EXPECT_TRUE(s.get("b").isNull());
    EXPECT_TRUE(s.hasKey("c"));
    EXPECT_TRUE(s.get("c").isNull());
  }

  {
    VPackSlice s = prepareResult("{\"a\":null,\"b\":1,\"c\":true}", out);
    EXPECT_EQ(3, s.length());
    EXPECT_TRUE(s.get("a").isNull());
    EXPECT_TRUE(s.get("b").isInteger());
    EXPECT_TRUE(s.get("c").getBool());
  }

  {
    VPackSlice s = prepareResult("{\"a\":1,\"b\":false,\"z\":\"foo\"}", out);
    EXPECT_EQ(3, s.length());
    EXPECT_TRUE(s.get("a").isInteger());
    EXPECT_FALSE(s.get("b").getBool());
    EXPECT_TRUE(s.get("c").isNull());
  }

  {
    VPackSlice s = prepareResult(
        "{\"a\":{\"subA\":1},\"b\":{\"subB\":false},\"c\":{\"subC\":\"foo\"}}",
        out);
    EXPECT_EQ(3, s.length());
    EXPECT_TRUE(s.get("a").isObject());
    EXPECT_TRUE(s.get({"a", "subA"}).isInteger());
    EXPECT_TRUE(s.get("b").isObject());
    EXPECT_TRUE(s.get({"b", "subB"}).isBoolean());
    EXPECT_TRUE(s.get("c").isObject());
    EXPECT_TRUE(s.get({"c", "subC"}).isString());
  }
}

TEST(ProjectionsTest, toVelocyPackFromDocumentComplex) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      createAttributeNamePath({"a", "b", "c"}, resMonitor),
      createAttributeNamePath({"a", "b", "d"}, resMonitor),
      createAttributeNamePath({"a", "c"}, resMonitor),
      createAttributeNamePath({"a", "d", "e", "f"}, resMonitor),
      createAttributeNamePath({"a", "z"}, resMonitor),
  };
  Projections p(std::move(attributes));

  velocypack::Builder out;

  auto prepareResult = [&p](std::string_view json, velocypack::Builder& out) {
    auto document = velocypack::Parser::fromJson(json.data(), json.size());
    out.clear();
    out.openObject();
    p.toVelocyPackFromDocument(out, document->slice(), nullptr);
    out.close();
    return out.slice();
  };

  {
    VPackSlice s = prepareResult("{}", out);
    EXPECT_EQ(1, s.length());
    EXPECT_TRUE(s.hasKey("a"));
    EXPECT_TRUE(s.get("a").isNull());
  }

  {
    VPackSlice s = prepareResult("{\"z\":1}", out);
    EXPECT_EQ(1, s.length());
    EXPECT_TRUE(s.hasKey("a"));
    EXPECT_TRUE(s.get("a").isNull());
  }

  {
    VPackSlice s = prepareResult("{\"a\":null}", out);
    EXPECT_EQ(1, s.length());
    EXPECT_TRUE(s.hasKey("a"));
    EXPECT_TRUE(s.get("a").isNull());
  }

  {
    VPackSlice s = prepareResult("{\"a\":1}", out);
    EXPECT_EQ(1, s.length());
    EXPECT_TRUE(s.hasKey("a"));
    EXPECT_TRUE(s.get("a").isInteger());
  }

  {
    VPackSlice s = prepareResult("{\"a\":\"foo\"}", out);
    EXPECT_EQ(1, s.length());
    EXPECT_TRUE(s.hasKey("a"));
    EXPECT_TRUE(s.get("a").isString());
  }

  {
    VPackSlice s = prepareResult(
        "{\"a\":{\"b\":{\"c\":1,\"d\":2},\"c\":\"foo\",\"d\":{\"e\":\"fuxx\"},"
        "\"z\":\"raton\"}}",
        out);
    EXPECT_EQ(1, s.length());
    EXPECT_TRUE(s.hasKey("a"));
    EXPECT_TRUE(s.get("a").isObject());
    EXPECT_TRUE(s.get({"a", "b"}).isObject());
    EXPECT_TRUE(s.get({"a", "b", "c"}).isInteger());
    EXPECT_TRUE(s.get({"a", "b", "d"}).isInteger());
    EXPECT_TRUE(s.get({"a", "c"}).isString());
    EXPECT_TRUE(s.get({"a", "d", "e"}).isString());
    EXPECT_TRUE(s.get({"a", "d", "f"}).isNone());
    EXPECT_TRUE(s.get({"a", "z"}).isString());
  }

  {
    VPackSlice s = prepareResult(
        "{\"a\":{\"b\":{\"c\":1,\"d\":2},\"c\":\"foo\",\"d\":{\"e\":{\"f\":4}},"
        "\"z\":\"raton\"}}",
        out);
    EXPECT_EQ(1, s.length());
    EXPECT_TRUE(s.hasKey("a"));
    EXPECT_TRUE(s.get("a").isObject());
    EXPECT_TRUE(s.get({"a", "b"}).isObject());
    EXPECT_TRUE(s.get({"a", "b", "c"}).isInteger());
    EXPECT_TRUE(s.get({"a", "b", "d"}).isInteger());
    EXPECT_TRUE(s.get({"a", "c"}).isString());
    EXPECT_TRUE(s.get({"a", "d"}).isObject());
    EXPECT_TRUE(s.get({"a", "d", "e", "f"}).isInteger());
    EXPECT_TRUE(s.get({"a", "z"}).isString());
  }
}

TEST(ProjectionsTest, toVelocyPackFromIndexSimple) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  mocks::MockAqlServer server;
  auto& vocbase = server.getSystemDatabase();
  auto collectionJson = velocypack::Parser::fromJson("{\"name\":\"test\"}");
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());

  bool created;
  auto indexJson = velocypack::Parser::fromJson(
      "{\"type\":\"hash\", \"fields\":[\"a\", \"b\"]}");
  auto index =
      logicalCollection->createIndex(indexJson->slice(), created).waitAndGet();

  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      createAttributeNamePath({"a"}, resMonitor),
      createAttributeNamePath({"b"}, resMonitor),
  };
  Projections p(std::move(attributes));

  p.setCoveringContext(DataSourceId(1), index);

  velocypack::Builder out;

  auto prepareResult = [&p](std::string_view json, velocypack::Builder& out) {
    auto document = velocypack::Parser::fromJson(json.data(), json.size());
    out.clear();
    out.openObject();
    auto data = IndexIterator::SliceCoveringData(document->slice());
    p.toVelocyPackFromIndex(out, data, nullptr);
    out.close();
    return out.slice();
  };

  EXPECT_TRUE(index->covers(p));

  {
    VPackSlice s = prepareResult("[null,1]", out);
    EXPECT_EQ(2, s.length());
    EXPECT_TRUE(s.get("a").isNull());
    EXPECT_TRUE(s.get("b").isInteger());
  }

  {
    VPackSlice s = prepareResult("[\"1234\",true]", out);
    EXPECT_EQ(2, s.length());
    EXPECT_TRUE(s.get("a").isString());
    EXPECT_TRUE(s.get("b").getBool());
  }
}

TEST(ProjectionsTest, toVelocyPackFromIndexComplex1) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  mocks::MockAqlServer server;
  auto& vocbase = server.getSystemDatabase();
  auto collectionJson = velocypack::Parser::fromJson("{\"name\":\"test\"}");
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());

  bool created;
  auto indexJson = velocypack::Parser::fromJson(
      "{\"type\":\"hash\", \"fields\":[\"sub.a\", \"sub.b\", \"c\"]}");
  auto index =
      logicalCollection->createIndex(indexJson->slice(), created).waitAndGet();

  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      createAttributeNamePath({"sub", "a"}, resMonitor),
      createAttributeNamePath({"sub", "b"}, resMonitor),
      createAttributeNamePath({"c"}, resMonitor),
  };
  Projections p(std::move(attributes));

  p.setCoveringContext(DataSourceId(1), index);

  velocypack::Builder out;

  auto prepareResult = [&p](std::string_view json, velocypack::Builder& out) {
    auto document = velocypack::Parser::fromJson(json.data(), json.size());
    out.clear();
    out.openObject();
    auto data = IndexIterator::SliceCoveringData(document->slice());
    p.toVelocyPackFromIndex(out, data, nullptr);
    out.close();
    return out.slice();
  };

  EXPECT_TRUE(index->covers(p));

  {
    VPackSlice s = prepareResult("[\"foo\",\"bar\",false]", out);
    EXPECT_EQ(2, s.length());
    EXPECT_TRUE(s.get({"sub", "a"}).isString());
    EXPECT_TRUE(s.get({"sub", "b"}).isString());
    EXPECT_FALSE(s.get("c").getBool());
  }
}

TEST(ProjectionsTest, toVelocyPackFromIndexComplex2) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  mocks::MockAqlServer server;
  auto& vocbase = server.getSystemDatabase();
  auto collectionJson = velocypack::Parser::fromJson("{\"name\":\"test\"}");
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());

  bool created;
  auto indexJson = velocypack::Parser::fromJson(
      "{\"type\":\"hash\", \"fields\":[\"sub\", \"c\"]}");
  auto index =
      logicalCollection->createIndex(indexJson->slice(), created).waitAndGet();

  std::vector<arangodb::aql::AttributeNamePath> attributes = {
      createAttributeNamePath({"sub", "a"}, resMonitor),
      createAttributeNamePath({"sub", "b"}, resMonitor),
      createAttributeNamePath({"c"}, resMonitor),
  };
  Projections p(std::move(attributes));

  p.setCoveringContext(DataSourceId(1), index);

  velocypack::Builder out;

  auto prepareResult = [&p](std::string_view json, velocypack::Builder& out) {
    auto document = velocypack::Parser::fromJson(json.data(), json.size());
    out.clear();
    out.openObject();
    auto data = IndexIterator::SliceCoveringData(document->slice());
    p.toVelocyPackFromIndex(out, data, nullptr);
    out.close();
    return out.slice();
  };

  EXPECT_TRUE(index->covers(p));

  {
    VPackSlice s = prepareResult("[{\"a\":\"foo\",\"b\":\"bar\"},false]", out);
    EXPECT_EQ(2, s.length());
    EXPECT_TRUE(s.get({"sub", "a"}).isString());
    EXPECT_TRUE(s.get({"sub", "b"}).isString());
    EXPECT_FALSE(s.get("c").getBool());
  }
}
