////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "catch.hpp"

#include "IResearch/IResearchViewSort.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"

TEST_CASE("IResearchViewSortTest", "[iresearch][iresearch-viewsort]") {

SECTION("test_defaults") {
  arangodb::iresearch::IResearchViewSort sort;
  CHECK(sort.empty());
  CHECK(0 == sort.size());
  CHECK(sort.memory() > 0);

  arangodb::velocypack::Builder builder;
  CHECK(!sort.toVelocyPack(builder));
  {
    arangodb::velocypack::ArrayBuilder arrayScope(&builder);
    CHECK(sort.toVelocyPack(builder));
  }
  auto slice = builder.slice();
  CHECK(slice.isArray());
  CHECK(0 == slice.length());
}

SECTION("equality") {
  arangodb::iresearch::IResearchViewSort lhs;
  lhs.emplace_back({ { "some", false }, { "Nested", false }, { "field", false } }, true);
  lhs.emplace_back({ { "another", false }, { "field", false } }, false);
  lhs.emplace_back({ { "simpleField", false } }, true);
  CHECK(lhs == lhs);
  CHECK(lhs != arangodb::iresearch::IResearchViewSort());

  {
    arangodb::iresearch::IResearchViewSort rhs;
    rhs.emplace_back({ { "some", false }, { "Nested", false }, { "field", false } }, true);
    rhs.emplace_back({ { "another", false }, { "field", false } }, false);
    CHECK(lhs != rhs);
  }

  {
    arangodb::iresearch::IResearchViewSort rhs;
    rhs.emplace_back({ { "some", false }, { "Nested", false }, { "field", false } }, true);
    rhs.emplace_back({ { "another", false }, { "field", false } }, false);
    rhs.emplace_back({ { "simpleField", false } }, false);
    CHECK(lhs != rhs);
  }

  {
    arangodb::iresearch::IResearchViewSort rhs;
    rhs.emplace_back({ { "some", false }, { "Nested", false }, { "field", false } }, true);
    rhs.emplace_back({ { "another", false }, { "fielD", false } }, false);
    rhs.emplace_back({ { "simpleField", false } }, true);
    CHECK(lhs != rhs);
  }
}

SECTION("deserialize") {
  arangodb::iresearch::IResearchViewSort sort;

  {
    auto json = arangodb::velocypack::Parser::fromJson("{ }");
    std::string errorField;
    CHECK(false == sort.fromVelocyPack(json->slice(), errorField));
    CHECK(errorField.empty());
  }

  {
    auto json = arangodb::velocypack::Parser::fromJson("[ ]");
    std::string errorField;
    CHECK(true == sort.fromVelocyPack(json->slice(), errorField));
    CHECK(errorField.empty());
    CHECK(sort.empty());
    CHECK(0 == sort.size());
    CHECK(sort.memory() > 0);
  }

  {
    auto json = arangodb::velocypack::Parser::fromJson("[ [ ] ]");
    std::string errorField;
    CHECK(false == sort.fromVelocyPack(json->slice(), errorField));
    CHECK("[0]" ==  errorField);
    CHECK(sort.empty());
    CHECK(0 == sort.size());
    CHECK(sort.memory() > 0);
  }

  {
    auto json = arangodb::velocypack::Parser::fromJson("[ { } ]");
    std::string errorField;
    CHECK(false == sort.fromVelocyPack(json->slice(), errorField));
    CHECK("[0]" == errorField);
    CHECK(sort.empty());
    CHECK(0 == sort.size());
    CHECK(sort.memory() > 0);
  }

  {
    auto json = arangodb::velocypack::Parser::fromJson("[ { \"field\": \"my.nested.field\" } ]");
    std::string errorField;
    CHECK(false == sort.fromVelocyPack(json->slice(), errorField));
    CHECK("[0]" == errorField);
    CHECK(sort.empty());
    CHECK(0 == sort.size());
    CHECK(sort.memory() > 0);
  }

  {
    auto json = arangodb::velocypack::Parser::fromJson("[ { \"field\": \"my.nested.field\", \"direction\": \"asc\" } ]");
    std::string errorField;
    CHECK(true == sort.fromVelocyPack(json->slice(), errorField));
    CHECK(errorField.empty());
    CHECK(!sort.empty());
    CHECK(1 == sort.size());
    CHECK(sort.memory() > 0);
    CHECK(sort.field(0) == std::vector<arangodb::basics::AttributeName>{ { "my", false }, { "nested", false }, { "field", false } });
    CHECK(true == sort.direction(0));
  }

  {
    auto json = arangodb::velocypack::Parser::fromJson("[ \
      { \"field\": \"my.nested.field\", \"direction\": \"asc\" }, \
      { \"field\": \"my.nested.field\", \"direction\": \"desc\" }, \
      { \"field\": \"another.nested.field\", \"asc\": false }, \
      { \"field\": \"yet.another.nested.field\", \"asc\": true } \
    ]");

    std::string errorField;
    CHECK(true == sort.fromVelocyPack(json->slice(), errorField));
    CHECK(errorField.empty());
    CHECK(!sort.empty());
    CHECK(4 == sort.size());
    CHECK(sort.memory() > 0);
    CHECK(std::vector<arangodb::basics::AttributeName>{ { "my", false }, { "nested", false }, { "field", false } } == sort.field(0));
    CHECK(true == sort.direction(0));
    CHECK(std::vector<arangodb::basics::AttributeName>{ { "my", false }, { "nested", false }, { "field", false } } == sort.field(1));
    CHECK(false == sort.direction(1));
    CHECK(std::vector<arangodb::basics::AttributeName>{ { "another", false }, { "nested", false }, { "field", false } } == sort.field(2));
    CHECK(false == sort.direction(2));
    CHECK(std::vector<arangodb::basics::AttributeName>{ { "yet", false }, { "another", false }, { "nested", false }, { "field", false } } == sort.field(3));
    CHECK(true == sort.direction(3));
  }

  {
    auto json = arangodb::velocypack::Parser::fromJson("[ \
      { \"field\": 1, \"direction\": \"asc\" }, \
      { \"field\": \"my.nested.field\", \"direction\": \"desc\" }, \
      { \"field\": \"another.nested.field\", \"asc\": false }, \
      { \"field\": \"yet.another.nested.field\", \"asc\": true } \
    ]");
    std::string errorField;
    CHECK(false == sort.fromVelocyPack(json->slice(), errorField));
    CHECK("[0]=>field" == errorField);
    CHECK(sort.empty());
    CHECK(0 == sort.size());
    CHECK(sort.memory() > 0);
  }
  
  {
    auto json = arangodb::velocypack::Parser::fromJson("[ \
      { \"field\": \"my.nested.field\", \"direction\": \"dasc\" }, \
      { \"field\": \"my.nested.field\", \"direction\": \"desc\" }, \
      { \"field\": \"another.nested.field\", \"asc\": false }, \
      { \"field\": \"yet.another.nested.field\", \"asc\": true } \
    ]");
    std::string errorField;
    CHECK(false == sort.fromVelocyPack(json->slice(), errorField));
    CHECK("[0]=>direction" == errorField);
    CHECK(sort.empty());
    CHECK(0 == sort.size());
    CHECK(sort.memory() > 0);
  }
  
  {
    auto json = arangodb::velocypack::Parser::fromJson("[ \
      { \"field\": \"my.nested.field\", \"direction\": \"asc\" }, \
      { \"field\": \"my.nested.field\", \"direction\": \"fdesc\" }, \
      { \"field\": \"another.nested.field\", \"asc\": false }, \
      { \"field\": \"yet.another.nested.field\", \"asc\": true } \
    ]");
    std::string errorField;
    CHECK(false == sort.fromVelocyPack(json->slice(), errorField));
    CHECK("[1]=>direction" == errorField);
  }

  {
    auto json = arangodb::velocypack::Parser::fromJson("[ \
      { \"field\": \"my.nested.field\", \"direction\": \"asc\" }, \
      { \"field\": \"my.nested.field\", \"direction\": \"desc\" }, \
      { \"field\": \"another.nested.field\", \"asc\": \"false\" }, \
      { \"field\": \"yet.another.nested.field\", \"asc\": true } \
    ]");
    std::string errorField;
    CHECK(false == sort.fromVelocyPack(json->slice(), errorField));
    CHECK("[2]=>asc" == errorField);
  }
}

SECTION("serialize") {
  arangodb::iresearch::IResearchViewSort sort;
  sort.emplace_back({ { "some", false }, { "Nested", false }, { "field", false } }, true);
  sort.emplace_back({ { "another", false }, { "field", false } }, false);
  sort.emplace_back({ { "simpleField", false } }, true);

  CHECK(!sort.empty());
  CHECK(3 == sort.size());
  CHECK(sort.memory() > 0);

  arangodb::velocypack::Builder builder;
  CHECK(!sort.toVelocyPack(builder));
  {
    arangodb::velocypack::ArrayBuilder arrayScope(&builder);
    CHECK(sort.toVelocyPack(builder));
  }
  auto slice = builder.slice();
  CHECK(slice.isArray());
  CHECK(3 == slice.length());

  arangodb::velocypack::ArrayIterator it(slice);
  CHECK(it.valid());

  {
    auto sortSlice = *it;
    CHECK(sortSlice.isObject());
    CHECK(2 == sortSlice.length());
    auto const fieldSlice = sortSlice.get("field");
    CHECK(fieldSlice.isString());
    CHECK("some.Nested.field" == fieldSlice.copyString());
    auto const directionSlice = sortSlice.get("asc");
    CHECK(directionSlice.isBoolean());
    CHECK(directionSlice.getBoolean());
  }

  it.next();
  CHECK(it.valid());

  {
    auto sortSlice = *it;
    CHECK(sortSlice.isObject());
    CHECK(2 == sortSlice.length());
    auto const fieldSlice = sortSlice.get("field");
    CHECK(fieldSlice.isString());
    CHECK("another.field" == fieldSlice.copyString());
    auto const directionSlice = sortSlice.get("asc");
    CHECK(directionSlice.isBoolean());
    CHECK(!directionSlice.getBoolean());
  }

  it.next();
  CHECK(it.valid());

  {
    auto sortSlice = *it;
    CHECK(sortSlice.isObject());
    CHECK(2 == sortSlice.length());
    auto const fieldSlice = sortSlice.get("field");
    CHECK(fieldSlice.isString());
    CHECK("simpleField" == fieldSlice.copyString());
    auto const directionSlice = sortSlice.get("asc");
    CHECK(directionSlice.isBoolean());
    CHECK(directionSlice.getBoolean());
  }

  it.next();
  CHECK(!it.valid());

  sort.clear();
  CHECK(sort.empty());
  CHECK(0 == sort.size());
  CHECK(sort.memory() > 0);
  {
    arangodb::velocypack::Builder builder;
    CHECK(!sort.toVelocyPack(builder));
    {
      arangodb::velocypack::ArrayBuilder arrayScope(&builder);
      CHECK(sort.toVelocyPack(builder));
    }
    auto slice = builder.slice();
    CHECK(slice.isArray());
    CHECK(0 == slice.length());
  }
}

}
