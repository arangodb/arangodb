////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for AttributeNamePath class
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
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

TEST(AttributeNamePathTest, hash) {
  EXPECT_EQ(17816462594913330409U, AttributeNamePath("_id").hash());
  EXPECT_EQ(12022528804101920381U, AttributeNamePath("_key").hash());
  EXPECT_EQ(2578897936239198297U, AttributeNamePath("_from").hash());
  EXPECT_EQ(16855074258498962665U, AttributeNamePath("_to").hash());
  EXPECT_EQ(13269526755862706111U, AttributeNamePath("_rev").hash());
  EXPECT_EQ(16172794815300636078U, AttributeNamePath("peter").hash());
  EXPECT_EQ(13480354774586618312U, AttributeNamePath("").hash());
  EXPECT_EQ(3417200101823867740U, AttributeNamePath("key").hash());
  EXPECT_EQ(1753109973746505091U, AttributeNamePath("id").hash());
  EXPECT_EQ(3712298677190048360U, AttributeNamePath("1").hash());
  EXPECT_EQ(12165985666513641907U, AttributeNamePath(std::vector<std::string>{ "a", "b" }).hash());
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
