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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "IResearch/IResearchViewSort.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"

TEST(IResearchViewSortTest, test_defaults) {
  arangodb::iresearch::IResearchViewSort sort;
  EXPECT_TRUE(sort.empty());
  EXPECT_EQ(0, sort.size());
  EXPECT_TRUE(sort.memory() > 0);

  arangodb::velocypack::Builder builder;
  EXPECT_FALSE(sort.toVelocyPack(builder));
  {
    arangodb::velocypack::ArrayBuilder arrayScope(&builder);
    EXPECT_TRUE(sort.toVelocyPack(builder));
  }
  auto slice = builder.slice();
  EXPECT_TRUE(slice.isArray());
  EXPECT_EQ(0, slice.length());
}

TEST(IResearchViewSortTest, equality) {
  arangodb::iresearch::IResearchViewSort lhs;
  lhs.emplace_back({{std::string_view("some"), false},
                    {std::string_view("Nested"), false},
                    {std::string_view("field"), false}},
                   true);
  lhs.emplace_back({{std::string_view("another"), false},
                    {std::string_view("field"), false}},
                   false);
  lhs.emplace_back({{std::string_view("simpleField"), false}}, true);
  EXPECT_EQ(lhs, lhs);
  EXPECT_NE(lhs, arangodb::iresearch::IResearchViewSort());

  {
    arangodb::iresearch::IResearchViewSort rhs;
    rhs.emplace_back({{std::string_view("some"), false},
                      {std::string_view("Nested"), false},
                      {std::string_view("field"), false}},
                     true);
    rhs.emplace_back({{std::string_view("another"), false},
                      {std::string_view("field"), false}},
                     false);
    EXPECT_NE(lhs, rhs);
  }

  {
    arangodb::iresearch::IResearchViewSort rhs;
    rhs.emplace_back({{std::string_view("some"), false},
                      {std::string_view("Nested"), false},
                      {std::string_view("field"), false}},
                     true);
    rhs.emplace_back({{std::string_view("another"), false},
                      {std::string_view("field"), false}},
                     false);
    rhs.emplace_back({{std::string_view("simpleField"), false}}, false);
    EXPECT_NE(lhs, rhs);
  }

  {
    arangodb::iresearch::IResearchViewSort rhs;
    rhs.emplace_back({{std::string_view("some"), false},
                      {std::string_view("Nested"), false},
                      {std::string_view("field"), false}},
                     true);
    rhs.emplace_back({{std::string_view("another"), false},
                      {std::string_view("fielD"), false}},
                     false);
    rhs.emplace_back({{std::string_view("simpleField"), false}}, true);
    EXPECT_NE(lhs, rhs);
  }
}

TEST(IResearchViewSortTest, deserialize) {
  arangodb::iresearch::IResearchViewSort sort;

  {
    auto json = arangodb::velocypack::Parser::fromJson("{ }");
    std::string errorField;
    EXPECT_FALSE(sort.fromVelocyPack(json->slice(), errorField));
    EXPECT_TRUE(errorField.empty());
  }

  {
    auto json = arangodb::velocypack::Parser::fromJson("[ ]");
    std::string errorField;
    EXPECT_TRUE(sort.fromVelocyPack(json->slice(), errorField));
    EXPECT_TRUE(errorField.empty());
    EXPECT_TRUE(sort.empty());
    EXPECT_EQ(0, sort.size());
    EXPECT_TRUE(sort.memory() > 0);
  }

  {
    auto json = arangodb::velocypack::Parser::fromJson("[ [ ] ]");
    std::string errorField;
    EXPECT_FALSE(sort.fromVelocyPack(json->slice(), errorField));
    EXPECT_EQ("[0]", errorField);
    EXPECT_TRUE(sort.empty());
    EXPECT_EQ(0, sort.size());
    EXPECT_TRUE(sort.memory() > 0);
  }

  {
    auto json = arangodb::velocypack::Parser::fromJson("[ { } ]");
    std::string errorField;
    EXPECT_FALSE(sort.fromVelocyPack(json->slice(), errorField));
    EXPECT_EQ("[0]", errorField);
    EXPECT_TRUE(sort.empty());
    EXPECT_EQ(0, sort.size());
    EXPECT_TRUE(sort.memory() > 0);
  }

  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "[ { \"field\": \"my.nested.field\" } ]");
    std::string errorField;
    EXPECT_FALSE(sort.fromVelocyPack(json->slice(), errorField));
    EXPECT_EQ("[0]", errorField);
    EXPECT_TRUE(sort.empty());
    EXPECT_EQ(0, sort.size());
    EXPECT_TRUE(sort.memory() > 0);
  }

  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "[ { \"field\": \"my.nested.field\", \"direction\": \"asc\" } ]");
    std::string errorField;
    EXPECT_TRUE(sort.fromVelocyPack(json->slice(), errorField));
    EXPECT_TRUE(errorField.empty());
    EXPECT_FALSE(sort.empty());
    EXPECT_EQ(1, sort.size());
    EXPECT_TRUE(sort.memory() > 0);
    EXPECT_TRUE((sort.field(0) == std::vector<arangodb::basics::AttributeName>{
                                      {std::string_view("my"), false},
                                      {std::string_view("nested"), false},
                                      {std::string_view("field"), false}}));
    EXPECT_TRUE(sort.direction(0));
  }

  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "[ \
      { \"field\": \"my.nested.field\", \"direction\": \"asc\" }, \
      { \"field\": \"my.nested.field\", \"direction\": \"desc\" }, \
      { \"field\": \"another.nested.field\", \"asc\": false }, \
      { \"field\": \"yet.another.nested.field\", \"asc\": true } \
    ]");

    std::string errorField;
    EXPECT_TRUE(sort.fromVelocyPack(json->slice(), errorField));
    EXPECT_TRUE(errorField.empty());
    EXPECT_FALSE(sort.empty());
    EXPECT_EQ(4, sort.size());
    EXPECT_TRUE(sort.memory() > 0);
    EXPECT_TRUE((std::vector<arangodb::basics::AttributeName>{
                     {std::string_view("my"), false},
                     {std::string_view("nested"), false},
                     {std::string_view("field"), false}} == sort.field(0)));
    EXPECT_TRUE(sort.direction(0));
    EXPECT_TRUE((std::vector<arangodb::basics::AttributeName>{
                     {std::string_view("my"), false},
                     {std::string_view("nested"), false},
                     {std::string_view("field"), false}} == sort.field(1)));
    EXPECT_FALSE(sort.direction(1));
    EXPECT_TRUE((std::vector<arangodb::basics::AttributeName>{
                     {std::string_view("another"), false},
                     {std::string_view("nested"), false},
                     {std::string_view("field"), false}} == sort.field(2)));
    EXPECT_FALSE(sort.direction(2));
    EXPECT_TRUE((std::vector<arangodb::basics::AttributeName>{
                     {std::string_view("yet"), false},
                     {std::string_view("another"), false},
                     {std::string_view("nested"), false},
                     {std::string_view("field"), false}} == sort.field(3)));
    EXPECT_TRUE(sort.direction(3));
  }

  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "[ \
      { \"field\": 1, \"direction\": \"asc\" }, \
      { \"field\": \"my.nested.field\", \"direction\": \"desc\" }, \
      { \"field\": \"another.nested.field\", \"asc\": false }, \
      { \"field\": \"yet.another.nested.field\", \"asc\": true } \
    ]");
    std::string errorField;
    EXPECT_FALSE(sort.fromVelocyPack(json->slice(), errorField));
    EXPECT_EQ("[0].field", errorField);
    EXPECT_TRUE(sort.empty());
    EXPECT_EQ(0, sort.size());
    EXPECT_TRUE(sort.memory() > 0);
  }

  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "[ \
      { \"field\": \"my.nested.field\", \"direction\": \"dasc\" }, \
      { \"field\": \"my.nested.field\", \"direction\": \"desc\" }, \
      { \"field\": \"another.nested.field\", \"asc\": false }, \
      { \"field\": \"yet.another.nested.field\", \"asc\": true } \
    ]");
    std::string errorField;
    EXPECT_FALSE(sort.fromVelocyPack(json->slice(), errorField));
    EXPECT_EQ("[0].direction", errorField);
    EXPECT_TRUE(sort.empty());
    EXPECT_EQ(0, sort.size());
    EXPECT_TRUE(sort.memory() > 0);
  }

  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "[ \
      { \"field\": \"my.nested.field\", \"direction\": \"asc\" }, \
      { \"field\": \"my.nested.field\", \"direction\": \"fdesc\" }, \
      { \"field\": \"another.nested.field\", \"asc\": false }, \
      { \"field\": \"yet.another.nested.field\", \"asc\": true } \
    ]");
    std::string errorField;
    EXPECT_FALSE(sort.fromVelocyPack(json->slice(), errorField));
    EXPECT_EQ("[1].direction", errorField);
  }

  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "[ \
      { \"field\": \"my.nested.field\", \"direction\": \"asc\" }, \
      { \"field\": \"my.nested.field\", \"direction\": \"desc\" }, \
      { \"field\": \"another.nested.field\", \"asc\": \"false\" }, \
      { \"field\": \"yet.another.nested.field\", \"asc\": true } \
    ]");
    std::string errorField;
    EXPECT_FALSE(sort.fromVelocyPack(json->slice(), errorField));
    EXPECT_EQ("[2].asc", errorField);
  }
}

TEST(IResearchViewSortTest, serialize) {
  arangodb::iresearch::IResearchViewSort sort;
  sort.emplace_back({{std::string_view("some"), false},
                     {std::string_view("Nested"), false},
                     {std::string_view("field"), false}},
                    true);
  sort.emplace_back({{std::string_view("another"), false},
                     {std::string_view("field"), false}},
                    false);
  sort.emplace_back({{std::string_view("simpleField"), false}}, true);

  EXPECT_FALSE(sort.empty());
  EXPECT_EQ(3, sort.size());
  EXPECT_TRUE(sort.memory() > 0);

  arangodb::velocypack::Builder builder;
  EXPECT_FALSE(sort.toVelocyPack(builder));
  {
    arangodb::velocypack::ArrayBuilder arrayScope(&builder);
    EXPECT_TRUE(sort.toVelocyPack(builder));
  }
  auto slice = builder.slice();
  EXPECT_TRUE(slice.isArray());
  EXPECT_EQ(3, slice.length());

  arangodb::velocypack::ArrayIterator it(slice);
  EXPECT_TRUE(it.valid());

  {
    auto sortSlice = *it;
    EXPECT_TRUE(sortSlice.isObject());
    EXPECT_EQ(2, sortSlice.length());
    auto const fieldSlice = sortSlice.get("field");
    EXPECT_TRUE(fieldSlice.isString());
    EXPECT_EQ("some.Nested.field", fieldSlice.copyString());
    auto const directionSlice = sortSlice.get("asc");
    EXPECT_TRUE(directionSlice.isBoolean());
    EXPECT_TRUE(directionSlice.getBoolean());
  }

  it.next();
  EXPECT_TRUE(it.valid());

  {
    auto sortSlice = *it;
    EXPECT_TRUE(sortSlice.isObject());
    EXPECT_EQ(2, sortSlice.length());
    auto const fieldSlice = sortSlice.get("field");
    EXPECT_TRUE(fieldSlice.isString());
    EXPECT_EQ("another.field", fieldSlice.copyString());
    auto const directionSlice = sortSlice.get("asc");
    EXPECT_TRUE(directionSlice.isBoolean());
    EXPECT_FALSE(directionSlice.getBoolean());
  }

  it.next();
  EXPECT_TRUE(it.valid());

  {
    auto sortSlice = *it;
    EXPECT_TRUE(sortSlice.isObject());
    EXPECT_EQ(2, sortSlice.length());
    auto const fieldSlice = sortSlice.get("field");
    EXPECT_TRUE(fieldSlice.isString());
    EXPECT_EQ("simpleField", fieldSlice.copyString());
    auto const directionSlice = sortSlice.get("asc");
    EXPECT_TRUE(directionSlice.isBoolean());
    EXPECT_TRUE(directionSlice.getBoolean());
  }

  it.next();
  EXPECT_FALSE(it.valid());

  sort.clear();
  EXPECT_TRUE(sort.empty());
  EXPECT_EQ(0, sort.size());
  EXPECT_TRUE(sort.memory() > 0);
  {
    arangodb::velocypack::Builder builder;
    EXPECT_FALSE(sort.toVelocyPack(builder));
    {
      arangodb::velocypack::ArrayBuilder arrayScope(&builder);
      EXPECT_TRUE(sort.toVelocyPack(builder));
    }
    auto slice = builder.slice();
    EXPECT_TRUE(slice.isArray());
    EXPECT_EQ(0, slice.length());
  }
}
