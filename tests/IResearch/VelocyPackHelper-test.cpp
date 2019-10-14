//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include <unordered_set>

#include "IResearch/VelocyPackHelper.h"

#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

TEST(IResearchVelocyPackHelperTest, test_defaults) {
  arangodb::iresearch::ObjectIterator it;
  EXPECT_EQ(0U, it.depth());
  EXPECT_FALSE(it.valid());
  EXPECT_EQ(arangodb::iresearch::ObjectIterator(), it);

  size_t calls_count = 0;
  auto visitor = [&calls_count](arangodb::iresearch::IteratorValue const&) {
    ++calls_count;
  };
  it.visit(visitor);
  EXPECT_EQ(0U, calls_count);
  // we not able to move the invalid iterator forward
}

TEST(IResearchVelocyPackHelperTest, test_getstring) {
  // string value
  {
    auto json =
        arangodb::velocypack::Parser::fromJson("{ \"key\": \"value\" }");
    auto slice = json->slice();
    std::string buf0;
    irs::string_ref buf1;
    bool seen;

    EXPECT_TRUE(
        (arangodb::iresearch::getString(buf0, slice, "key", seen, "abc")));
    EXPECT_TRUE(seen);
    EXPECT_EQ(buf0, "value");

    EXPECT_TRUE(
        (arangodb::iresearch::getString(buf1, slice, "key", seen, "abc")));
    EXPECT_TRUE(seen);
    EXPECT_EQ(buf1, "value");
  }

  // missing key
  {
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    auto slice = json->slice();
    std::string buf0;
    irs::string_ref buf1;
    bool seen = true;

    EXPECT_TRUE(
        (arangodb::iresearch::getString(buf0, slice, "key", seen, "abc")));
    EXPECT_FALSE(seen);
    EXPECT_EQ(buf0, "abc");

    seen = true;

    EXPECT_TRUE(
        (arangodb::iresearch::getString(buf1, slice, "key", seen, "abc")));
    EXPECT_FALSE(seen);
    EXPECT_EQ(buf1, "abc");
  }

  // non-string value
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"key\": 12345 }");
    auto slice = json->slice();
    std::string buf0;
    irs::string_ref buf1;
    bool seen;

    EXPECT_TRUE(
        (!arangodb::iresearch::getString(buf0, slice, "key", seen, "abc")));
    EXPECT_TRUE(seen);

    EXPECT_TRUE(
        (!arangodb::iresearch::getString(buf1, slice, "key", seen, "abc")));
    EXPECT_TRUE(seen);
  }
}

TEST(IResearchVelocyPackHelperTest, test_empty_object) {
  auto json = arangodb::velocypack::Parser::fromJson("{ }");
  auto slice = json->slice();

  arangodb::iresearch::ObjectIterator it(slice);

  EXPECT_EQ(1U, it.depth());
  EXPECT_TRUE(it.valid());
  EXPECT_EQ(arangodb::iresearch::ObjectIterator(slice), it);

  auto& value = it.value(0);  // value at level 0
  EXPECT_EQ(0U, value.pos);
  EXPECT_EQ(VPackValueType::Object, value.type);
  EXPECT_TRUE(value.key.isNone());
  EXPECT_TRUE(value.value.isNone());
  EXPECT_EQ(&value, &*it);

  ++it;

  EXPECT_EQ(0U, it.depth());
  EXPECT_FALSE(it.valid());
  EXPECT_EQ(arangodb::iresearch::ObjectIterator(), it);
}

TEST(IResearchVelocyPackHelperTest, test_subarray_of_emptyobjects) {
  auto json = arangodb::velocypack::Parser::fromJson("[ {}, {}, {} ]");
  auto slice = json->slice();

  arangodb::iresearch::ObjectIterator it(slice);

  EXPECT_EQ(2U, it.depth());
  EXPECT_TRUE(it.valid());
  EXPECT_EQ(arangodb::iresearch::ObjectIterator(slice), it);

  // check value at level 0
  {
    auto& value = it.value(0);
    EXPECT_EQ(0U, value.pos);
    EXPECT_EQ(VPackValueType::Array, value.type);
    EXPECT_TRUE(value.key.isObject());
    EXPECT_TRUE(value.value.isObject());
  }

  // check value at level 1
  {
    auto& value = it.value(1);
    EXPECT_EQ(0U, value.pos);
    EXPECT_EQ(VPackValueType::Object, value.type);
    EXPECT_TRUE(value.key.isNone());
    EXPECT_TRUE(value.value.isNone());
    EXPECT_EQ(&value, &*it);
  }

  {
    auto const prev = it;
    EXPECT_EQ(prev, it++);
  }

  // check value at level 0
  {
    auto& value = it.value(0);
    EXPECT_EQ(1U, value.pos);
    EXPECT_EQ(VPackValueType::Array, value.type);
    EXPECT_TRUE(value.key.isObject());
    EXPECT_TRUE(value.value.isObject());
  }

  // check value at level 1
  {
    auto& value = it.value(1);
    EXPECT_EQ(0U, value.pos);
    EXPECT_EQ(VPackValueType::Object, value.type);
    EXPECT_TRUE(value.key.isNone());
    EXPECT_TRUE(value.value.isNone());
    EXPECT_EQ(&value, &*it);
  }

  ++it;

  // check value at level 0
  {
    auto& value = it.value(0);
    EXPECT_EQ(2U, value.pos);
    EXPECT_EQ(VPackValueType::Array, value.type);
    EXPECT_TRUE(value.key.isObject());
    EXPECT_TRUE(value.value.isObject());
  }

  // check value at level 1
  {
    auto& value = it.value(1);
    EXPECT_EQ(0U, value.pos);
    EXPECT_EQ(VPackValueType::Object, value.type);
    EXPECT_TRUE(value.key.isNone());
    EXPECT_TRUE(value.value.isNone());
    EXPECT_EQ(&value, &*it);
  }

  {
    auto const prev = it;
    EXPECT_EQ(prev, it++);
  }

  EXPECT_EQ(0U, it.depth());
  EXPECT_FALSE(it.valid());
  EXPECT_EQ(arangodb::iresearch::ObjectIterator(), it);
}

TEST(IResearchVelocyPackHelperTest, test_small_plain_object) {
  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"boost\": \"10\" \
  }");
  auto slice = json->slice();

  arangodb::iresearch::ObjectIterator it(slice);

  EXPECT_EQ(1U, it.depth());
  EXPECT_TRUE(it.valid());
  EXPECT_EQ(arangodb::iresearch::ObjectIterator(slice), it);

  auto& value = *it;

  EXPECT_EQ(0U, value.pos);
  EXPECT_EQ(VPackValueType::Object, value.type);
  EXPECT_TRUE(value.key.isString());
  EXPECT_EQ("boost", value.key.copyString());
  EXPECT_TRUE(value.value.isString());
  EXPECT_EQ("10", value.value.copyString());

  ++it;

  EXPECT_EQ(0U, it.depth());
  EXPECT_FALSE(it.valid());
  EXPECT_EQ(arangodb::iresearch::ObjectIterator(), it);
}

TEST(IResearchVelocyPackHelperTest, test_empty_subarray) {
  auto json = arangodb::velocypack::Parser::fromJson("[ [ [ ] ] ]");
  auto slice = json->slice();

  arangodb::iresearch::ObjectIterator it(slice);

  EXPECT_EQ(3U, it.depth());
  EXPECT_TRUE(it.valid());
  EXPECT_EQ(arangodb::iresearch::ObjectIterator(slice), it);

  // check that ObjectIterator::visit & ObjectIterator::value operates on the same values
  {
    bool result = true;
    size_t level = 0;
    auto check_levels = [&it, level,
                         &result](arangodb::iresearch::IteratorValue const& value) mutable {
      result &= (&(it.value(level++)) == &value);
    };
    it.visit(check_levels);
    EXPECT_TRUE(result);
  }

  // check value at level 0
  {
    auto& value = it.value(0);
    EXPECT_EQ(0U, value.pos);
    EXPECT_EQ(VPackValueType::Array, value.type);
    EXPECT_TRUE(value.key.isArray());
    EXPECT_TRUE(value.value.isArray());
  }

  // check value at level 1
  {
    auto& value = it.value(1);
    EXPECT_EQ(0U, value.pos);
    EXPECT_EQ(VPackValueType::Array, value.type);
    EXPECT_TRUE(value.key.isArray());
    EXPECT_TRUE(value.value.isArray());
  }

  // check value at level 2
  {
    auto& value = it.value(2);
    EXPECT_EQ(0U, value.pos);
    EXPECT_EQ(VPackValueType::Array, value.type);
    EXPECT_TRUE(value.key.isNone());
    EXPECT_TRUE(value.value.isNone());
    EXPECT_EQ(&value, &*it);
  }

  ++it;

  EXPECT_EQ(0U, it.depth());
  EXPECT_FALSE(it.valid());
  EXPECT_EQ(arangodb::iresearch::ObjectIterator(), it);
}

TEST(IResearchVelocyPackHelperTest, test_empty_subobject) {
  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"sub0\" : { \"sub1\" : { } } }");
  auto slice = json->slice();

  arangodb::iresearch::ObjectIterator it(slice);

  EXPECT_EQ(3U, it.depth());
  EXPECT_TRUE(it.valid());
  EXPECT_EQ(arangodb::iresearch::ObjectIterator(slice), it);

  // check that ObjectIterator::visit & ObjectIterator::value operates on the same values
  {
    bool result = true;
    size_t level = 0;
    auto check_levels = [&it, level,
                         &result](arangodb::iresearch::IteratorValue const& value) mutable {
      result &= (&(it.value(level++)) == &value);
    };
    it.visit(check_levels);
    EXPECT_TRUE(result);
  }

  // check value at level 0
  {
    auto& value = it.value(0);
    EXPECT_EQ(0U, value.pos);
    EXPECT_EQ(VPackValueType::Object, value.type);
    EXPECT_TRUE(value.key.isString());
    EXPECT_EQ("sub0", value.key.copyString());
    EXPECT_TRUE(value.value.isObject());
  }

  // check value at level 1
  {
    auto& value = it.value(1);
    EXPECT_EQ(0U, value.pos);
    EXPECT_EQ(VPackValueType::Object, value.type);
    EXPECT_TRUE(value.key.isString());
    EXPECT_EQ("sub1", value.key.copyString());
    EXPECT_TRUE(value.value.isObject());
  }

  // check value at level 2
  {
    auto& value = it.value(2);
    EXPECT_EQ(0U, value.pos);
    EXPECT_EQ(VPackValueType::Object, value.type);
    EXPECT_TRUE(value.key.isNone());
    EXPECT_TRUE(value.value.isNone());
    EXPECT_EQ(&value, &*it);
  }

  ++it;

  EXPECT_EQ(0U, it.depth());
  EXPECT_FALSE(it.valid());
  EXPECT_EQ(arangodb::iresearch::ObjectIterator(), it);
}

TEST(IResearchVelocyPackHelperTest, test_empty_array) {
  auto json = arangodb::velocypack::Parser::fromJson("[ ]");
  auto slice = json->slice();

  arangodb::iresearch::ObjectIterator it(slice);

  EXPECT_EQ(1U, it.depth());
  EXPECT_TRUE(it.valid());
  EXPECT_EQ(arangodb::iresearch::ObjectIterator(slice), it);

  auto& value = it.value(0);  // value at level 0
  EXPECT_EQ(0U, value.pos);
  EXPECT_EQ(VPackValueType::Array, value.type);
  EXPECT_TRUE(value.key.isNone());
  EXPECT_TRUE(value.value.isNone());
  EXPECT_EQ(&value, &*it);

  ++it;

  EXPECT_EQ(0U, it.depth());
  EXPECT_FALSE(it.valid());
  EXPECT_EQ(arangodb::iresearch::ObjectIterator(), it);
}

TEST(IResearchVelocyPackHelperTest, test_complex_object) {
  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"nested\": { \"foo\": \"str\" }, \
    \"keys\": [ \"1\",\"2\",\"3\",\"4\" ], \
    \"analyzers\": {}, \
    \"boost\": \"10\", \
    \"depth\": \"20\", \
    \"fields\": { \"fieldA\" : { \"name\" : \"a\" }, \"fieldB\" : { \"name\" : \"b\" } }, \
    \"listValuation\": \"ignored\", \
    \"locale\": \"ru_RU.KOI8-R\", \
    \"array\" : [ \
      { \"id\" : \"1\", \"subarr\" : [ \"1\", \"2\", \"3\" ], \"subobj\" : { \"id\" : \"1\" } }, \
      { \"subarr\" : [ \"4\", \"5\", \"6\" ], \"subobj\" : { \"name\" : \"foo\" }, \"id\" : \"2\" }, \
      { \"id\" : \"3\", \"subarr\" : [ \"7\", \"8\", \"9\" ], \"subobj\" : { \"id\" : \"2\" } } \
    ] \
  }");

  std::unordered_multiset<std::string> expectedValues{
      "nested{0}.foo{0}=str",
      "keys{1}[0]=1",
      "keys{1}[1]=2",
      "keys{1}[2]=3",
      "keys{1}[3]=4",
      "analyzers{2}=",
      "boost{3}=10",
      "depth{4}=20",
      "fields{5}.fieldA{0}.name{0}=a",
      "fields{5}.fieldB{1}.name{0}=b",
      "listValuation{6}=ignored",
      "locale{7}=ru_RU.KOI8-R",

      "array{8}[0].id{0}=1",
      "array{8}[0].subarr{1}[0]=1",
      "array{8}[0].subarr{1}[1]=2",
      "array{8}[0].subarr{1}[2]=3",
      "array{8}[0].subobj{2}.id{0}=1",

      "array{8}[1].subarr{0}[0]=4",
      "array{8}[1].subarr{0}[1]=5",
      "array{8}[1].subarr{0}[2]=6",
      "array{8}[1].subobj{1}.name{0}=foo",
      "array{8}[1].id{2}=2",

      "array{8}[2].id{0}=3",
      "array{8}[2].subarr{1}[0]=7",
      "array{8}[2].subarr{1}[1]=8",
      "array{8}[2].subarr{1}[2]=9",
      "array{8}[2].subobj{2}.id{0}=2",
  };

  auto slice = json->slice();

  std::string name;
  auto visitor = [&name](arangodb::iresearch::IteratorValue const& value) {
    if (value.type == VPackValueType::Array) {
      name += '[';
      name += std::to_string(value.pos);
      name += ']';
    } else if (value.type == VPackValueType::Object) {
      if (!value.key.isString()) {
        return;
      }

      if (!name.empty()) {
        name += '.';
      }

      name += value.key.copyString();
      name += '{';
      name += std::to_string(value.pos);
      name += '}';
    }
  };

  for (arangodb::iresearch::ObjectIterator it(slice); it.valid(); ++it) {
    it.visit(visitor);
    name += '=';

    auto& value = *it;
    if (value.value.isString()) {
      name += value.value.copyString();
    }

    EXPECT_TRUE(expectedValues.erase(name) > 0);

    name.clear();
  }

  EXPECT_TRUE(expectedValues.empty());
}
