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

#include "Aql/AttributeNamePath.h"

using namespace arangodb::aql;

TEST(AttributeNamePathTest, empty) {
  AttributeNamePath p;
  ASSERT_TRUE(p.empty());
  ASSERT_EQ(0, p.size());

  p.path.emplace_back("test");
  ASSERT_FALSE(p.empty());
  ASSERT_EQ(1, p.size());
}

TEST(AttributeNamePathTest, size) {
  AttributeNamePath p;
  ASSERT_EQ(0, p.size());

  for (size_t i = 0; i < 10; ++i) {
    p.path.emplace_back("test");
    ASSERT_EQ(i + 1, p.size());
  }
}

TEST(AttributeNamePathTest, type) {
  ASSERT_EQ(AttributeNamePath::Type::IdAttribute, AttributeNamePath("_id").type());
  ASSERT_EQ(AttributeNamePath::Type::KeyAttribute, AttributeNamePath("_key").type());
  ASSERT_EQ(AttributeNamePath::Type::FromAttribute, AttributeNamePath("_from").type());
  ASSERT_EQ(AttributeNamePath::Type::ToAttribute, AttributeNamePath("_to").type());
  ASSERT_EQ(AttributeNamePath::Type::SingleAttribute, AttributeNamePath("_rev").type());
  ASSERT_EQ(AttributeNamePath::Type::SingleAttribute, AttributeNamePath("peter").type());
  ASSERT_EQ(AttributeNamePath::Type::SingleAttribute, AttributeNamePath("").type());
  ASSERT_EQ(AttributeNamePath::Type::SingleAttribute, AttributeNamePath("key").type());
  ASSERT_EQ(AttributeNamePath::Type::SingleAttribute, AttributeNamePath("id").type());
  ASSERT_EQ(AttributeNamePath::Type::SingleAttribute, AttributeNamePath("1").type());
  ASSERT_EQ(AttributeNamePath::Type::MultiAttribute, AttributeNamePath(std::vector<std::string>{ "a", "b" }).type());
}

/*
 * note: these hash values are based on std::hash(), so they can differ
 * across platforms or compiler versions. it is not safe to enable them
 */ 
TEST(AttributeNamePathTest, hash) {
  EXPECT_EQ(15615448896660710867U, AttributeNamePath("_id").hash());
  EXPECT_EQ(10824270243171927571U, AttributeNamePath("_key").hash());
  EXPECT_EQ(14053392154382459499U, AttributeNamePath("_from").hash());
  EXPECT_EQ(13320164962300719886U, AttributeNamePath("_to").hash());
  EXPECT_EQ(17306043388796241266U, AttributeNamePath("_rev").hash());
  EXPECT_EQ(6428928981455593224U, AttributeNamePath("peter").hash());
  EXPECT_EQ(3023579531467661406U, AttributeNamePath("").hash());
  EXPECT_EQ(14833537490909846811U, AttributeNamePath("key").hash());
  EXPECT_EQ(9633197497402266361U, AttributeNamePath("id").hash());
  EXPECT_EQ(17812082371732153461U, AttributeNamePath("1").hash());
  EXPECT_EQ(14488091495141700494U, AttributeNamePath(std::vector<std::string>{ "a", "b" }).hash());
  EXPECT_EQ(8328517039192094391U, AttributeNamePath(std::vector<std::string>{ "b", "a" }).hash());
}

TEST(AttributeNamePathTest, atLong) {
  AttributeNamePath p;
  p.path.emplace_back("foo");
  p.path.emplace_back("bar");
  p.path.emplace_back("baz");

  ASSERT_EQ("foo", p[0]);
  ASSERT_EQ("bar", p[1]);
  ASSERT_EQ("baz", p[2]);
}
TEST(AttributeNamePathTest, atShort) {
  AttributeNamePath p{ "foobar" };

  ASSERT_EQ("foobar", p[0]);
}

TEST(AttributeNamePathTest, equalsLong) {
  AttributeNamePath p1;
  p1.path.emplace_back("foo");
  p1.path.emplace_back("bar");
  p1.path.emplace_back("baz");

  AttributeNamePath p2;
  p2.path.emplace_back("foo");
  p2.path.emplace_back("bar");
  p2.path.emplace_back("baz");

  ASSERT_TRUE(p1 == p2);
  ASSERT_FALSE(p1 != p2);
  
  p1.path.pop_back();
  ASSERT_FALSE(p1 == p2);
  ASSERT_TRUE(p1 != p2);
  
  p2.path.pop_back();
  ASSERT_TRUE(p1 == p2);
  ASSERT_FALSE(p1 != p2);
}
  
TEST(AttributeNamePathTest, equalsShort) {
  ASSERT_TRUE(AttributeNamePath("_id") == AttributeNamePath("_id"));
  ASSERT_FALSE(AttributeNamePath("_id") != AttributeNamePath("_id"));
  ASSERT_FALSE(AttributeNamePath("_id") == AttributeNamePath("_key"));
  ASSERT_TRUE(AttributeNamePath("_id") != AttributeNamePath("_key"));
  ASSERT_FALSE(AttributeNamePath("_from") == AttributeNamePath("_key"));
  ASSERT_TRUE(AttributeNamePath("_from") != AttributeNamePath("_key"));
  ASSERT_FALSE(AttributeNamePath("_key") == AttributeNamePath("_from"));
  ASSERT_TRUE(AttributeNamePath("_key") != AttributeNamePath("_from"));
  
  ASSERT_TRUE(AttributeNamePath("_key") == AttributeNamePath("_key"));
  ASSERT_FALSE(AttributeNamePath("_key") != AttributeNamePath("_key"));
  ASSERT_TRUE(AttributeNamePath("_key") != AttributeNamePath("_id"));
  ASSERT_FALSE(AttributeNamePath("_key") == AttributeNamePath("_id"));
  ASSERT_FALSE(AttributeNamePath("_id") == AttributeNamePath("_key"));
  ASSERT_TRUE(AttributeNamePath("_id") != AttributeNamePath("_key"));
  
  ASSERT_TRUE(AttributeNamePath(std::vector<std::string>{ "a", "b" }) == 
              AttributeNamePath(std::vector<std::string>{ "a", "b" }));
  
  ASSERT_FALSE(AttributeNamePath(std::vector<std::string>{ "b", "a" }) == 
               AttributeNamePath(std::vector<std::string>{ "a", "b" }));
  
  ASSERT_FALSE(AttributeNamePath(std::vector<std::string>{ "b" }) == 
               AttributeNamePath(std::vector<std::string>{ "a", "b" }));
  
  ASSERT_FALSE(AttributeNamePath(std::vector<std::string>{ "a" }) == 
               AttributeNamePath(std::vector<std::string>{ "a", "b" }));
}

TEST(AttributeNamePathTest, less) {
  ASSERT_FALSE(AttributeNamePath("_id") < AttributeNamePath("_id"));
  ASSERT_TRUE(AttributeNamePath("_id") < AttributeNamePath("_key"));
  ASSERT_TRUE(AttributeNamePath("_from") < AttributeNamePath("_key"));
  ASSERT_FALSE(AttributeNamePath("_key") < AttributeNamePath("_from"));
  ASSERT_FALSE(AttributeNamePath("_key") < AttributeNamePath("_key"));
  ASSERT_FALSE(AttributeNamePath("_key") < AttributeNamePath("_id"));
  
  ASSERT_FALSE(AttributeNamePath("a") < AttributeNamePath("a"));
  ASSERT_TRUE(AttributeNamePath("a") < AttributeNamePath("b"));
  ASSERT_TRUE(AttributeNamePath("A") < AttributeNamePath("a"));
  ASSERT_FALSE(AttributeNamePath("A") < AttributeNamePath("A"));
  
  ASSERT_FALSE(AttributeNamePath(std::vector<std::string>{ "a", "b" }) < 
               AttributeNamePath(std::vector<std::string>{ "a", "b" }));
  
  ASSERT_FALSE(AttributeNamePath(std::vector<std::string>{ "b", "a" }) < 
               AttributeNamePath(std::vector<std::string>{ "a", "b" }));
  
  ASSERT_TRUE(AttributeNamePath(std::vector<std::string>{ "a", "b" }) < 
              AttributeNamePath(std::vector<std::string>{ "b", "a" }));
  
  ASSERT_FALSE(AttributeNamePath(std::vector<std::string>{ "b" }) < 
               AttributeNamePath(std::vector<std::string>{ "a", "b" }));
  
  ASSERT_TRUE(AttributeNamePath(std::vector<std::string>{ "a", "b" }) < 
              AttributeNamePath(std::vector<std::string>{ "b" }));
  
  ASSERT_TRUE(AttributeNamePath(std::vector<std::string>{ "a" }) < 
              AttributeNamePath(std::vector<std::string>{ "a", "b" }));
  
  ASSERT_FALSE(AttributeNamePath(std::vector<std::string>{ "a", "b" }) < 
               AttributeNamePath(std::vector<std::string>{ "a" }));
}

TEST(AttributeNamePathTest, reverse) {
  ASSERT_EQ(AttributeNamePath("abc"), AttributeNamePath("abc").reverse());
  
  ASSERT_EQ(AttributeNamePath(std::vector<std::string>{ "b", "a" }),
            AttributeNamePath(std::vector<std::string>{ "a", "b" }).reverse());
  
  ASSERT_EQ(AttributeNamePath(std::vector<std::string>{ "a", "a" }),
            AttributeNamePath(std::vector<std::string>{ "a", "a" }).reverse());
  
  ASSERT_EQ(AttributeNamePath(std::vector<std::string>{ "ab", "cde", "fgh", "ihj" }),
            AttributeNamePath(std::vector<std::string>{ "ihj", "fgh", "cde", "ab" }).reverse());
}

TEST(AttributeNamePathTest, shortenTo) {
  ASSERT_EQ(AttributeNamePath("abc"), AttributeNamePath("abc").shortenTo(10));
  ASSERT_EQ(AttributeNamePath("abc"), AttributeNamePath("abc").shortenTo(2));
  ASSERT_EQ(AttributeNamePath("abc"), AttributeNamePath("abc").shortenTo(1));
  ASSERT_EQ(AttributeNamePath(), AttributeNamePath("abc").shortenTo(0));
  
  ASSERT_EQ(AttributeNamePath(std::vector<std::string>{}), AttributeNamePath(std::vector<std::string>{"a", "b", "c", "d"}).shortenTo(0));
  ASSERT_EQ(AttributeNamePath(std::vector<std::string>{"a"}), AttributeNamePath(std::vector<std::string>{"a", "b", "c", "d"}).shortenTo(1));
  ASSERT_EQ(AttributeNamePath(std::vector<std::string>{"a", "b"}), AttributeNamePath(std::vector<std::string>{"a", "b", "c", "d"}).shortenTo(2));
  ASSERT_EQ(AttributeNamePath(std::vector<std::string>{"a", "b", "c"}), AttributeNamePath(std::vector<std::string>{"a", "b", "c", "d"}).shortenTo(3));
  ASSERT_EQ(AttributeNamePath(std::vector<std::string>{"a", "b", "c", "d"}), AttributeNamePath(std::vector<std::string>{"a", "b", "c", "d"}).shortenTo(4));
  ASSERT_EQ(AttributeNamePath(std::vector<std::string>{"a", "b", "c", "d"}), AttributeNamePath(std::vector<std::string>{"a", "b", "c", "d"}).shortenTo(5));
  ASSERT_EQ(AttributeNamePath(std::vector<std::string>{"a", "b", "c", "d"}), AttributeNamePath(std::vector<std::string>{"a", "b", "c", "d"}).shortenTo(1000));
}

TEST(AttributeNamePathTest, commonPrefixLength) {
  ASSERT_EQ(1, AttributeNamePath::commonPrefixLength(AttributeNamePath("abc"), AttributeNamePath("abc")));
  ASSERT_EQ(0, AttributeNamePath::commonPrefixLength(AttributeNamePath("abc"), AttributeNamePath("piff")));
  ASSERT_EQ(0, AttributeNamePath::commonPrefixLength(AttributeNamePath("a"), AttributeNamePath("b")));
  
  ASSERT_EQ(1, AttributeNamePath::commonPrefixLength(AttributeNamePath(std::vector<std::string>{"a"}), AttributeNamePath(std::vector<std::string>{"a", "b"})));
  ASSERT_EQ(1, AttributeNamePath::commonPrefixLength(AttributeNamePath(std::vector<std::string>{"a"}), AttributeNamePath(std::vector<std::string>{"a", "b", "c"})));
  ASSERT_EQ(2, AttributeNamePath::commonPrefixLength(AttributeNamePath(std::vector<std::string>{"a", "b"}), AttributeNamePath(std::vector<std::string>{"a", "b"})));
  ASSERT_EQ(2, AttributeNamePath::commonPrefixLength(AttributeNamePath(std::vector<std::string>{"a", "b"}), AttributeNamePath(std::vector<std::string>{"a", "b", "c"})));
  ASSERT_EQ(1, AttributeNamePath::commonPrefixLength(AttributeNamePath(std::vector<std::string>{"a", "b"}), AttributeNamePath(std::vector<std::string>{"a", "c", "b"})));
  ASSERT_EQ(0, AttributeNamePath::commonPrefixLength(AttributeNamePath(std::vector<std::string>{"a", "b"}), AttributeNamePath(std::vector<std::string>{"z", "a", "b"})));
  ASSERT_EQ(0, AttributeNamePath::commonPrefixLength(AttributeNamePath(std::vector<std::string>{"a"}), AttributeNamePath(std::vector<std::string>{"b", "a"})));
  
  ASSERT_EQ(1, AttributeNamePath::commonPrefixLength(AttributeNamePath(std::vector<std::string>{"a", "b"}), AttributeNamePath(std::vector<std::string>{"a"})));
  ASSERT_EQ(1, AttributeNamePath::commonPrefixLength(AttributeNamePath(std::vector<std::string>{"a", "b", "c"}), AttributeNamePath(std::vector<std::string>{"a"})));
  ASSERT_EQ(2, AttributeNamePath::commonPrefixLength(AttributeNamePath(std::vector<std::string>{"a", "b", "c"}), AttributeNamePath(std::vector<std::string>{"a", "b"})));
  ASSERT_EQ(1, AttributeNamePath::commonPrefixLength(AttributeNamePath(std::vector<std::string>{"a", "c", "b"}), AttributeNamePath(std::vector<std::string>{"a", "b"})));
  ASSERT_EQ(0, AttributeNamePath::commonPrefixLength(AttributeNamePath(std::vector<std::string>{"z", "a", "b"}), AttributeNamePath(std::vector<std::string>{"a", "b"})));
  ASSERT_EQ(0, AttributeNamePath::commonPrefixLength(AttributeNamePath(std::vector<std::string>{"b", "a"}), AttributeNamePath(std::vector<std::string>{"a"})));
}
