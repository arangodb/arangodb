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

#include "Basics/GlobalResourceMonitor.h"
#include "Basics/MemoryTypes/MemoryTypes.h"
#include "gtest/gtest.h"

#include "Aql/AttributeNamePath.h"

using namespace arangodb::aql;

auto createAttributeNamePath = [](std::vector<std::string>&& vec,
                                  arangodb::ResourceMonitor& resMonitor) {
  arangodb::MonitoredStringVector myVector{resMonitor};
  for (auto& s : vec) {
    myVector.emplace_back(s);
  }
  return AttributeNamePath(std::move(myVector), resMonitor);
};

TEST(AttributeNamePathTest, empty) {
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
  AttributeNamePath p{monitor};
  ASSERT_TRUE(p.empty());
  ASSERT_EQ(0, p.size());

  p._path.emplace_back("test");
  ASSERT_FALSE(p.empty());
  ASSERT_EQ(1, p.size());
}

TEST(AttributeNamePathTest, size) {
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
  AttributeNamePath p{monitor};
  ASSERT_EQ(0, p.size());

  for (size_t i = 0; i < 10; ++i) {
    p._path.emplace_back("test");
    ASSERT_EQ(i + 1, p.size());
  }
}

TEST(AttributeNamePathTest, type) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  ASSERT_EQ(AttributeNamePath::Type::IdAttribute,
            AttributeNamePath("_id", resMonitor).type());
  ASSERT_EQ(AttributeNamePath::Type::KeyAttribute,
            AttributeNamePath("_key", resMonitor).type());
  ASSERT_EQ(AttributeNamePath::Type::FromAttribute,
            AttributeNamePath("_from", resMonitor).type());
  ASSERT_EQ(AttributeNamePath::Type::ToAttribute,
            AttributeNamePath("_to", resMonitor).type());
  ASSERT_EQ(AttributeNamePath::Type::SingleAttribute,
            AttributeNamePath("_rev", resMonitor).type());
  ASSERT_EQ(AttributeNamePath::Type::SingleAttribute,
            AttributeNamePath("peter", resMonitor).type());
  ASSERT_EQ(AttributeNamePath::Type::SingleAttribute,
            AttributeNamePath("", resMonitor).type());
  ASSERT_EQ(AttributeNamePath::Type::SingleAttribute,
            AttributeNamePath("key", resMonitor).type());
  ASSERT_EQ(AttributeNamePath::Type::SingleAttribute,
            AttributeNamePath("id", resMonitor).type());
  ASSERT_EQ(AttributeNamePath::Type::SingleAttribute,
            AttributeNamePath("1", resMonitor).type());

  auto anp = createAttributeNamePath({"a", "b"}, resMonitor);
  ASSERT_EQ(AttributeNamePath::Type::MultiAttribute, anp.type());
}

/*
 * note: these hash values are based on std::hash(), so they can differ
 * across platforms or compiler versions. it is not safe to enable them
 */
TEST(AttributeNamePathTest, hash) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};

  EXPECT_EQ(15615448896660710867U, AttributeNamePath("_id", resMonitor).hash());
  EXPECT_EQ(10824270243171927571U,
            AttributeNamePath("_key", resMonitor).hash());
  EXPECT_EQ(14053392154382459499U,
            AttributeNamePath("_from", resMonitor).hash());
  EXPECT_EQ(13320164962300719886U, AttributeNamePath("_to", resMonitor).hash());
  EXPECT_EQ(17306043388796241266U,
            AttributeNamePath("_rev", resMonitor).hash());
  EXPECT_EQ(6428928981455593224U,
            AttributeNamePath("peter", resMonitor).hash());
  EXPECT_EQ(3023579531467661406U, AttributeNamePath("", resMonitor).hash());
  EXPECT_EQ(14833537490909846811U, AttributeNamePath("key", resMonitor).hash());
  EXPECT_EQ(9633197497402266361U, AttributeNamePath("id", resMonitor).hash());
  EXPECT_EQ(17812082371732153461U, AttributeNamePath("1", resMonitor).hash());

  auto anpAB = createAttributeNamePath({"a", "b"}, resMonitor);
  auto anpBA = createAttributeNamePath({"b", "a"}, resMonitor);
  EXPECT_EQ(14488091495141700494U, anpAB.hash());
  EXPECT_EQ(8328517039192094391U, anpBA.hash());
}

TEST(AttributeNamePathTest, atLong) {
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
  AttributeNamePath p{monitor};
  p._path.emplace_back("foo");
  p._path.emplace_back("bar");
  p._path.emplace_back("baz");

  ASSERT_EQ("foo", p[0]);
  ASSERT_EQ("bar", p[1]);
  ASSERT_EQ("baz", p[2]);
}
TEST(AttributeNamePathTest, atShort) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  AttributeNamePath p{"foobar", resMonitor};

  ASSERT_EQ("foobar", p[0]);
}

TEST(AttributeNamePathTest, equalsLong) {
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
  AttributeNamePath p1{monitor};
  p1._path.emplace_back("foo");
  p1._path.emplace_back("bar");
  p1._path.emplace_back("baz");

  AttributeNamePath p2{monitor};
  p2._path.emplace_back("foo");
  p2._path.emplace_back("bar");
  p2._path.emplace_back("baz");

  ASSERT_TRUE(p1 == p2);
  ASSERT_FALSE(p1 != p2);

  p1._path.pop_back();
  ASSERT_FALSE(p1 == p2);
  ASSERT_TRUE(p1 != p2);

  p2._path.pop_back();
  ASSERT_TRUE(p1 == p2);
  ASSERT_FALSE(p1 != p2);
}

TEST(AttributeNamePathTest, equalsShort) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  ASSERT_TRUE(AttributeNamePath("_id", resMonitor) ==
              AttributeNamePath("_id", resMonitor));
  ASSERT_FALSE(AttributeNamePath("_id", resMonitor) !=
               AttributeNamePath("_id", resMonitor));
  ASSERT_FALSE(AttributeNamePath("_id", resMonitor) ==
               AttributeNamePath("_key", resMonitor));
  ASSERT_TRUE(AttributeNamePath("_id", resMonitor) !=
              AttributeNamePath("_key", resMonitor));
  ASSERT_FALSE(AttributeNamePath("_from", resMonitor) ==
               AttributeNamePath("_key", resMonitor));
  ASSERT_TRUE(AttributeNamePath("_from", resMonitor) !=
              AttributeNamePath("_key", resMonitor));
  ASSERT_FALSE(AttributeNamePath("_key", resMonitor) ==
               AttributeNamePath("_from", resMonitor));
  ASSERT_TRUE(AttributeNamePath("_key", resMonitor) !=
              AttributeNamePath("_from", resMonitor));

  ASSERT_TRUE(AttributeNamePath("_key", resMonitor) ==
              AttributeNamePath("_key", resMonitor));
  ASSERT_FALSE(AttributeNamePath("_key", resMonitor) !=
               AttributeNamePath("_key", resMonitor));
  ASSERT_TRUE(AttributeNamePath("_key", resMonitor) !=
              AttributeNamePath("_id", resMonitor));
  ASSERT_FALSE(AttributeNamePath("_key", resMonitor) ==
               AttributeNamePath("_id", resMonitor));
  ASSERT_FALSE(AttributeNamePath("_id", resMonitor) ==
               AttributeNamePath("_key", resMonitor));
  ASSERT_TRUE(AttributeNamePath("_id", resMonitor) !=
              AttributeNamePath("_key", resMonitor));

  auto anpAB = createAttributeNamePath({"a", "b"}, resMonitor);
  ASSERT_TRUE(anpAB == anpAB);

  auto anpBA = createAttributeNamePath({"b", "a"}, resMonitor);
  ASSERT_FALSE(anpAB == anpBA);

  auto anpB = createAttributeNamePath({"b"}, resMonitor);
  ASSERT_FALSE(anpB == anpAB);

  auto anpA = createAttributeNamePath({"a"}, resMonitor);
  ASSERT_FALSE(anpA == anpAB);
}

TEST(AttributeNamePathTest, less) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  ASSERT_FALSE(AttributeNamePath("_id", resMonitor) <
               AttributeNamePath("_id", resMonitor));
  ASSERT_TRUE(AttributeNamePath("_id", resMonitor) <
              AttributeNamePath("_key", resMonitor));
  ASSERT_TRUE(AttributeNamePath("_from", resMonitor) <
              AttributeNamePath("_key", resMonitor));
  ASSERT_FALSE(AttributeNamePath("_key", resMonitor) <
               AttributeNamePath("_from", resMonitor));
  ASSERT_FALSE(AttributeNamePath("_key", resMonitor) <
               AttributeNamePath("_key", resMonitor));
  ASSERT_FALSE(AttributeNamePath("_key", resMonitor) <
               AttributeNamePath("_id", resMonitor));

  ASSERT_FALSE(AttributeNamePath("a", resMonitor) <
               AttributeNamePath("a", resMonitor));
  ASSERT_TRUE(AttributeNamePath("a", resMonitor) <
              AttributeNamePath("b", resMonitor));
  ASSERT_TRUE(AttributeNamePath("A", resMonitor) <
              AttributeNamePath("a", resMonitor));
  ASSERT_FALSE(AttributeNamePath("A", resMonitor) <
               AttributeNamePath("A", resMonitor));

  ASSERT_FALSE(createAttributeNamePath({"a", "b"}, resMonitor) <
               createAttributeNamePath({"a", "b"}, resMonitor));

  ASSERT_FALSE(createAttributeNamePath({"b", "a"}, resMonitor) <
               createAttributeNamePath({"a", "b"}, resMonitor));

  ASSERT_TRUE(createAttributeNamePath({"a", "b"}, resMonitor) <
              createAttributeNamePath({"b", "a"}, resMonitor));

  ASSERT_FALSE(createAttributeNamePath({"b"}, resMonitor) <
               createAttributeNamePath({"a", "b"}, resMonitor));

  ASSERT_TRUE(createAttributeNamePath({"a", "b"}, resMonitor) <
              createAttributeNamePath({"b"}, resMonitor));

  ASSERT_TRUE(createAttributeNamePath({"a"}, resMonitor) <
              createAttributeNamePath({"a", "b"}, resMonitor));

  ASSERT_FALSE(createAttributeNamePath({"a", "b"}, resMonitor) <
               createAttributeNamePath({"a"}, resMonitor));
}

TEST(AttributeNamePathTest, reverse) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  ASSERT_EQ(AttributeNamePath("abc", resMonitor),
            AttributeNamePath("abc", resMonitor).reverse());

  ASSERT_EQ(createAttributeNamePath({"b", "a"}, resMonitor),
            createAttributeNamePath({"a", "b"}, resMonitor).reverse());

  ASSERT_EQ(createAttributeNamePath({"a", "a"}, resMonitor),
            createAttributeNamePath({"a", "a"}, resMonitor).reverse());

  ASSERT_EQ(createAttributeNamePath({"ab", "cde", "fgh", "ihj"}, resMonitor),
            createAttributeNamePath({"ihj", "fgh", "cde", "ab"}, resMonitor)
                .reverse());
}

TEST(AttributeNamePathTest, shortenTo) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  ASSERT_EQ(AttributeNamePath("abc", resMonitor),
            AttributeNamePath("abc", resMonitor).shortenTo(10));
  ASSERT_EQ(AttributeNamePath("abc", resMonitor),
            AttributeNamePath("abc", resMonitor).shortenTo(2));
  ASSERT_EQ(AttributeNamePath("abc", resMonitor),
            AttributeNamePath("abc", resMonitor).shortenTo(1));
  ASSERT_EQ(AttributeNamePath(resMonitor),
            AttributeNamePath("abc", resMonitor).shortenTo(0));

  ASSERT_EQ(
      createAttributeNamePath({}, resMonitor),
      createAttributeNamePath({"a", "b", "c", "d"}, resMonitor).shortenTo(0));
  ASSERT_EQ(
      createAttributeNamePath({"a"}, resMonitor),
      createAttributeNamePath({"a", "b", "c", "d"}, resMonitor).shortenTo(1));
  ASSERT_EQ(
      createAttributeNamePath({"a", "b"}, resMonitor),
      createAttributeNamePath({"a", "b", "c", "d"}, resMonitor).shortenTo(2));
  ASSERT_EQ(
      createAttributeNamePath({"a", "b", "c"}, resMonitor),
      createAttributeNamePath({"a", "b", "c", "d"}, resMonitor).shortenTo(3));
  ASSERT_EQ(
      createAttributeNamePath({"a", "b", "c", "d"}, resMonitor),
      createAttributeNamePath({"a", "b", "c", "d"}, resMonitor).shortenTo(4));
  ASSERT_EQ(
      createAttributeNamePath({"a", "b", "c", "d"}, resMonitor),
      createAttributeNamePath({"a", "b", "c", "d"}, resMonitor).shortenTo(5));
  ASSERT_EQ(createAttributeNamePath({"a", "b", "c", "d"}, resMonitor),
            createAttributeNamePath({"a", "b", "c", "d"}, resMonitor)
                .shortenTo(1000));
}

TEST(AttributeNamePathTest, commonPrefixLength) {
  arangodb::GlobalResourceMonitor globalResourceMonitor{};
  arangodb::ResourceMonitor resMonitor{globalResourceMonitor};
  ASSERT_EQ(1, AttributeNamePath::commonPrefixLength(
                   AttributeNamePath("abc", resMonitor),
                   AttributeNamePath("abc", resMonitor)));
  ASSERT_EQ(0, AttributeNamePath::commonPrefixLength(
                   AttributeNamePath("abc", resMonitor),
                   AttributeNamePath("piff", resMonitor)));
  ASSERT_EQ(0, AttributeNamePath::commonPrefixLength(
                   AttributeNamePath("a", resMonitor),
                   AttributeNamePath("b", resMonitor)));

  ASSERT_EQ(1, AttributeNamePath::commonPrefixLength(
                   createAttributeNamePath({"a"}, resMonitor),
                   createAttributeNamePath({"a", "b"}, resMonitor)));
  ASSERT_EQ(1, AttributeNamePath::commonPrefixLength(
                   createAttributeNamePath({"a"}, resMonitor),
                   createAttributeNamePath({"a", "b", "c"}, resMonitor)));
  ASSERT_EQ(2, AttributeNamePath::commonPrefixLength(
                   createAttributeNamePath({"a", "b"}, resMonitor),
                   createAttributeNamePath({"a", "b"}, resMonitor)));
  ASSERT_EQ(2, AttributeNamePath::commonPrefixLength(
                   createAttributeNamePath({"a", "b"}, resMonitor),
                   createAttributeNamePath({"a", "b", "c"}, resMonitor)));
  ASSERT_EQ(1, AttributeNamePath::commonPrefixLength(
                   createAttributeNamePath({"a", "b"}, resMonitor),
                   createAttributeNamePath({"a", "c", "b"}, resMonitor)));
  ASSERT_EQ(0, AttributeNamePath::commonPrefixLength(
                   createAttributeNamePath({"a", "b"}, resMonitor),
                   createAttributeNamePath({"z", "a", "b"}, resMonitor)));
  ASSERT_EQ(0, AttributeNamePath::commonPrefixLength(
                   createAttributeNamePath({"a"}, resMonitor),
                   createAttributeNamePath({"b", "a"}, resMonitor)));

  ASSERT_EQ(1, AttributeNamePath::commonPrefixLength(
                   createAttributeNamePath({"a", "b"}, resMonitor),
                   createAttributeNamePath({"a"}, resMonitor)));
  ASSERT_EQ(1, AttributeNamePath::commonPrefixLength(
                   createAttributeNamePath({"a", "b", "c"}, resMonitor),
                   createAttributeNamePath({"a"}, resMonitor)));
  ASSERT_EQ(2, AttributeNamePath::commonPrefixLength(
                   createAttributeNamePath({"a", "b", "c"}, resMonitor),
                   createAttributeNamePath({"a", "b"}, resMonitor)));
  ASSERT_EQ(1, AttributeNamePath::commonPrefixLength(
                   createAttributeNamePath({"a", "c", "b"}, resMonitor),
                   createAttributeNamePath({"a", "b"}, resMonitor)));
  ASSERT_EQ(0, AttributeNamePath::commonPrefixLength(
                   createAttributeNamePath({"z", "a", "b"}, resMonitor),
                   createAttributeNamePath({"a", "b"}, resMonitor)));
  ASSERT_EQ(0, AttributeNamePath::commonPrefixLength(
                   createAttributeNamePath({"b", "a"}, resMonitor),
                   createAttributeNamePath({"a"}, resMonitor)));
}
