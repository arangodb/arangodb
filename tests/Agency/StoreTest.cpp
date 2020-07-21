////////////////////////////////////////////////////////////////////////////////
/// @brief test case for CleanOutServer job
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include "Agency/Store.h"

TEST(StoreTest, store_preconditions) {
  using namespace arangodb::consensus;

  Node node("node");
  Node::Children& children = node.children();
  VPackBuilder foo, baz, pi;
  foo.add(VPackValue("bar"));
  baz.add(VPackValue(13));
  pi.add(VPackValue(3.14159265359));
  auto fooNode = std::make_shared<Node>("foo");
  auto bazNode = std::make_shared<Node>("baz");
  auto piNode = std::make_shared<Node>("pi");
  *fooNode = foo.slice();
  *bazNode = baz.slice();
  *piNode = pi.slice();

  children.insert(std::make_pair("foo", fooNode));
  children.insert(std::make_pair("baz", bazNode));
  children.insert(std::make_pair("pi", piNode));
  children.insert(std::make_pair("foo1", fooNode));
  children.insert(std::make_pair("baz1", bazNode));
  children.insert(std::make_pair("pi1", piNode));

  VPackOptions opts;
  opts.buildUnindexedObjects = true;
  VPackBuilder other(&opts);
  {
    VPackObjectBuilder o(&other);
    other.add("pi1", VPackValue(3.14159265359));
    other.add("foo", VPackValue("bar"));
    other.add("pi", VPackValue(3.14159265359));
    other.add("baz1", VPackValue(13));
    other.add("foo1", VPackValue("bar"));
    other.add("baz", VPackValue(13));
  }

  ASSERT_EQ(node, other.slice());
}

TEST(StoreTest, store_split) {
  using namespace arangodb::consensus;

  ASSERT_EQ(std::vector<std::string>(), Store::split(""));
  ASSERT_EQ(std::vector<std::string>(), Store::split("/"));
  ASSERT_EQ(std::vector<std::string>(), Store::split("//"));
  ASSERT_EQ(std::vector<std::string>(), Store::split("///"));
  
  ASSERT_EQ((std::vector<std::string>{"a"}), Store::split("a"));
  ASSERT_EQ((std::vector<std::string>{"a c"}), Store::split("a c"));
  ASSERT_EQ((std::vector<std::string>{"foobar"}), Store::split("foobar"));
  ASSERT_EQ((std::vector<std::string>{"foo bar"}), Store::split("foo bar"));
  
  ASSERT_EQ((std::vector<std::string>{"a", "b", "c"}), Store::split("a/b/c"));
  ASSERT_EQ((std::vector<std::string>{"a", "b", "c"}), Store::split("/a/b/c"));
  ASSERT_EQ((std::vector<std::string>{"a", "b", "c"}), Store::split("/a/b/c/"));
  ASSERT_EQ((std::vector<std::string>{"a", "b", "c"}), Store::split("//a/b/c"));
  ASSERT_EQ((std::vector<std::string>{"a", "b", "c"}), Store::split("//a/b/c/"));
  ASSERT_EQ((std::vector<std::string>{"a", "b", "c"}), Store::split("a/b/c//"));
  ASSERT_EQ((std::vector<std::string>{"a", "b", "c"}), Store::split("//a/b/c//"));
  ASSERT_EQ((std::vector<std::string>{"a", "b", "c"}), Store::split("//a/b/c//"));
  ASSERT_EQ((std::vector<std::string>{"a"}), Store::split("//////a"));
  ASSERT_EQ((std::vector<std::string>{"a"}), Store::split("a//////////////"));
  ASSERT_EQ((std::vector<std::string>{"a"}), Store::split("/////////////a//////////////"));
  ASSERT_EQ((std::vector<std::string>{"foobar"}), Store::split("//////foobar"));
  ASSERT_EQ((std::vector<std::string>{"foobar"}), Store::split("foobar//////////////"));
  ASSERT_EQ((std::vector<std::string>{"foobar"}), Store::split("/////////////foobar//////////////"));
  ASSERT_EQ((std::vector<std::string>{"a", "c"}), Store::split("a/c"));
  ASSERT_EQ((std::vector<std::string>{"a", "c"}), Store::split("a//c"));
  ASSERT_EQ((std::vector<std::string>{"a", "c"}), Store::split("a///c"));
  ASSERT_EQ((std::vector<std::string>{"a", "c"}), Store::split("/a//c"));
  ASSERT_EQ((std::vector<std::string>{"a", "c"}), Store::split("/a//c"));
  ASSERT_EQ((std::vector<std::string>{"a", "c"}), Store::split("/a///c"));
  ASSERT_EQ((std::vector<std::string>{"a", "c"}), Store::split("/a//c/"));
  ASSERT_EQ((std::vector<std::string>{"a", "c"}), Store::split("/a//c//"));
  ASSERT_EQ((std::vector<std::string>{"a", "c"}), Store::split("/a///c//"));
  ASSERT_EQ((std::vector<std::string>{"foo", "bar"}), Store::split("foo/bar"));
  ASSERT_EQ((std::vector<std::string>{"foo", "bar"}), Store::split("foo//bar"));
  ASSERT_EQ((std::vector<std::string>{"foo", "bar"}), Store::split("foo///bar"));
  ASSERT_EQ((std::vector<std::string>{"foo", "bar"}), Store::split("/foo//bar"));
  ASSERT_EQ((std::vector<std::string>{"foo", "bar"}), Store::split("/foo//bar"));
  ASSERT_EQ((std::vector<std::string>{"foo", "bar"}), Store::split("/foo///bar"));
  ASSERT_EQ((std::vector<std::string>{"foo", "bar"}), Store::split("/foo//bar/"));
  ASSERT_EQ((std::vector<std::string>{"foo", "bar"}), Store::split("/foo//bar//"));
  ASSERT_EQ((std::vector<std::string>{"foo", "bar"}), Store::split("/foo///bar//"));
  
  ASSERT_EQ((std::vector<std::string>{"foo", "bar", "baz"}), Store::split("/foo///bar//baz"));
}

TEST(StoreTest, store_normalize) {
  using namespace arangodb::consensus;

  EXPECT_EQ("/", Store::normalize(""));
  EXPECT_EQ("/", Store::normalize("/"));
  EXPECT_EQ("/", Store::normalize("//"));
  EXPECT_EQ("/", Store::normalize("////"));
  EXPECT_EQ("/a", Store::normalize("a"));
  EXPECT_EQ("/a", Store::normalize("/a"));
  EXPECT_EQ("/a", Store::normalize("/a/"));
  EXPECT_EQ("/a", Store::normalize("//a/"));
  EXPECT_EQ("/a", Store::normalize("//a//"));
  EXPECT_EQ("/a/b", Store::normalize("a/b"));
  EXPECT_EQ("/a/b", Store::normalize("a/b/"));
  EXPECT_EQ("/a/b", Store::normalize("/a/b"));
  EXPECT_EQ("/a/b", Store::normalize("//a/b"));
  EXPECT_EQ("/a/b", Store::normalize("/a//b"));
  EXPECT_EQ("/a/b", Store::normalize("/a/b/"));
  EXPECT_EQ("/a/b", Store::normalize("/a/b//"));
  EXPECT_EQ("/a/b", Store::normalize("//a//b//"));
  EXPECT_EQ("/a/b/c", Store::normalize("a/b/c"));
  EXPECT_EQ("/a/b/c", Store::normalize("a/b/c/"));
  EXPECT_EQ("/a/b/c", Store::normalize("/a/b/c"));
  EXPECT_EQ("/a/b/c", Store::normalize("a//b/c"));
  EXPECT_EQ("/a/b/c", Store::normalize("a/b//c"));
  EXPECT_EQ("/mutter", Store::normalize("mutter"));
  EXPECT_EQ("/mutter", Store::normalize("/mutter"));
  EXPECT_EQ("/mutter", Store::normalize("//mutter"));
  EXPECT_EQ("/mutter", Store::normalize("mutter/"));
  EXPECT_EQ("/mutter", Store::normalize("mutter//"));
  EXPECT_EQ("/mutter", Store::normalize("/mutter//"));
  EXPECT_EQ("/mutter", Store::normalize("//mutter//"));
  EXPECT_EQ("/der/hund", Store::normalize("der/hund"));
  EXPECT_EQ("/der/hund", Store::normalize("/der/hund"));
  EXPECT_EQ("/der/hund", Store::normalize("der/hund/"));
  EXPECT_EQ("/der/hund", Store::normalize("/der/hund/"));
  EXPECT_EQ("/der/hund", Store::normalize("der/////hund"));
  EXPECT_EQ("/der/hund", Store::normalize("der/hund/////"));
  EXPECT_EQ("/der/hund", Store::normalize("////der/hund"));
  EXPECT_EQ("/der/hund/der/schwitzt", Store::normalize("der/hund/der/schwitzt"));
  EXPECT_EQ("/der/hund/der/schwitzt", Store::normalize("der/hund/der/schwitzt/"));
  EXPECT_EQ("/der/hund/der/schwitzt", Store::normalize("/der/hund/der/schwitzt/"));
}
