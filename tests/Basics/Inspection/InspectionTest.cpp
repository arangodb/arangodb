////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include <iostream>
#include <list>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

#include <velocypack/Value.h>
#include <velocypack/ValueType.h>
#include <velocypack/velocypack-memory.h>

#include "Inspection/Access.h"
#include "Inspection/Format.h"
#include "Inspection/Types.h"
#include "Inspection/VPack.h"
#include "Inspection/VPackLoadInspector.h"
#include "Inspection/VPackSaveInspector.h"
#include "Inspection/VPackWithErrorT.h"
#include "Inspection/ValidateInspector.h"
#include "velocypack/Builder.h"
#include "VelocypackUtils/VelocyPackStringLiteral.h"

#include "Logger/LogMacros.h"

#include "InspectionTestHelper.h"

namespace {
using namespace arangodb;

struct VPackInspectionTest : public ::testing::Test {};

TEST_F(VPackInspectionTest, serialize) {
  velocypack::Builder builder;
  Dummy const d{.i = 42, .d = 123.456, .b = true, .s = "foobar"};
  arangodb::velocypack::serialize(builder, d);

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  EXPECT_EQ(d.i, slice["i"].getInt());
  EXPECT_EQ(d.d, slice["d"].getDouble());
  EXPECT_EQ(d.b, slice["b"].getBool());
  EXPECT_EQ(d.s, slice["s"].copyString());
}

TEST_F(VPackInspectionTest, serialize_to_builder) {
  Dummy const d{.i = 42, .d = 123.456, .b = true, .s = "cheese"};
  auto sharedSlice = arangodb::velocypack::serialize(d);

  ASSERT_TRUE(sharedSlice.isObject());
  EXPECT_EQ(d.i, sharedSlice["i"].getInt());
  EXPECT_EQ(d.d, sharedSlice["d"].getDouble());
  EXPECT_EQ(d.b, sharedSlice["b"].getBool());
  EXPECT_EQ(d.s, sharedSlice["s"].copyString());
}

TEST_F(VPackInspectionTest, formatter) {
  auto d = Dummy{.i = 42, .d = 123.456, .b = true, .s = "cheese"};

  auto def = fmt::format("My name is {}", d);
  EXPECT_EQ(def,
            "My name is {\"i\":42,\"d\":123.456,\"b\":true,\"s\":\"cheese\"}");

  auto notPretty = fmt::format("My name is {:u}", d);
  EXPECT_EQ(notPretty,
            "My name is {\"i\":42,\"d\":123.456,\"b\":true,\"s\":\"cheese\"}");
  EXPECT_EQ(def, notPretty);

  auto pretty = fmt::format("My name is {:p}", d);
  EXPECT_EQ(pretty,
            "My name is {\n  \"i\" : 42,\n  \"d\" : 123.456,\n  \"b\" : "
            "true,\n  \"s\" : \"cheese\"\n}");
}

TEST_F(VPackInspectionTest, formatter_prints_serialization_error) {
  MyStringEnum val = static_cast<MyStringEnum>(42);
  auto def = fmt::format("{}", val);
  ASSERT_EQ(def, R"({"error":"Unknown enum value 42"})");
}

TEST_F(VPackInspectionTest, deserialize) {
  velocypack::Builder builder;
  builder.openObject();
  builder.add("i", VPackValue(42));
  builder.add("d", VPackValue(123.456));
  builder.add("b", VPackValue(true));
  builder.add("s", VPackValue("foobar"));
  builder.close();

  Dummy d = arangodb::velocypack::deserialize<Dummy>(builder.slice());
  EXPECT_EQ(42, d.i);
  EXPECT_EQ(123.456, d.d);
  EXPECT_EQ(true, d.b);
  EXPECT_EQ("foobar", d.s);
}

TEST_F(VPackInspectionTest, deserialize_throws) {
  velocypack::Builder builder;
  builder.openObject();
  builder.close();

  EXPECT_THROW(
      {
        try {
          arangodb::velocypack::deserialize<Dummy>(builder.slice());
        } catch (arangodb::basics::Exception& e) {
          EXPECT_TRUE(std::string(e.what()).starts_with(
              "Error while parsing VelocyPack: Missing required attribute"))
              << "Actual error message: " << e.what();
          throw;
        }
      },
      arangodb::basics::Exception);
}

TEST_F(VPackInspectionTest, GenericEnumClass) {
  {
    velocypack::Builder builder;

    AnEnumClass const d = AnEnumClass::Option1;
    arangodb::velocypack::serialize(builder, d);

    velocypack::Slice slice = builder.slice();
    ASSERT_TRUE(slice.isObject());
    EXPECT_EQ(static_cast<std::underlying_type_t<AnEnumClass>>(d),
              slice["code"].getInt());
    EXPECT_EQ(to_string(d), slice["message"].copyString());
  }

  {
    auto const expected = AnEnumClass::Option3;
    velocypack::Builder builder;

    builder.openObject();
    builder.add(
        "code",
        VPackValue(static_cast<std::underlying_type_t<AnEnumClass>>(expected)));
    builder.add("message", VPackValue(to_string(expected)));
    builder.close();

    auto d = arangodb::velocypack::deserialize<AnEnumClass>(builder.slice());

    EXPECT_EQ(d, expected);
  }
}

struct IncludesVPackBuilder {
  VPackBuilder builder;
};

template<typename Inspector>
auto inspect(Inspector& f, IncludesVPackBuilder& x) {
  return f.object(x).fields(f.field("builder", x.builder));
}

TEST_F(VPackInspectionTest, StructIncludingVPackBuilder) {
  VPackBuilder builder;
  builder.openObject();
  builder.add("key", "value");
  builder.close();
  auto const myStruct = IncludesVPackBuilder{.builder = builder};

  {
    VPackBuilder serializedMyStruct;
    serialize(serializedMyStruct, myStruct);

    auto slice = serializedMyStruct.slice();
    ASSERT_TRUE(slice.isObject());
    EXPECT_EQ("value", slice["builder"]["key"].copyString());
  }

  {
    VPackBuilder serializedMyStruct;
    serializedMyStruct.openObject();
    serializedMyStruct.add(VPackValue("builder"));
    serializedMyStruct.openObject();
    serializedMyStruct.add("key", "value");
    serializedMyStruct.close();
    serializedMyStruct.close();

    auto deserializedMyStruct =
        deserialize<IncludesVPackBuilder>(serializedMyStruct.slice());

    ASSERT_TRUE(deserializedMyStruct.builder.slice().binaryEquals(
        myStruct.builder.slice()));
  }
}

TEST_F(VPackInspectionTest, Result) {
  arangodb::Result result = {TRI_ERROR_INTERNAL, "some error message"};
  VPackBuilder expectedSerlized;
  {
    VPackObjectBuilder ob(&expectedSerlized);
    expectedSerlized.add("number", TRI_ERROR_INTERNAL);
    expectedSerlized.add("message", "some error message");
  }

  VPackBuilder serialized;
  arangodb::velocypack::serialize(serialized, result);
  auto slice = serialized.slice();
  EXPECT_EQ(expectedSerlized.toJson(), serialized.toJson());

  auto deserialized =
      arangodb::velocypack::deserialize<arangodb::Result>(slice);
  EXPECT_EQ(result, deserialized);
}

TEST_F(VPackInspectionTest, ResultTWithResultInside) {
  arangodb::ResultT<uint64_t> result =
      arangodb::Result{TRI_ERROR_INTERNAL, "some error message"};
  VPackBuilder expectedSerlized;
  {
    VPackObjectBuilder ob(&expectedSerlized);
    expectedSerlized.add(VPackValue("error"));
    {
      VPackObjectBuilder ob2(&expectedSerlized);
      expectedSerlized.add("number", TRI_ERROR_INTERNAL);
      expectedSerlized.add("message", "some error message");
    }
  }

  VPackBuilder serialized;
  arangodb::velocypack::serialize(serialized, result);
  auto slice = serialized.slice();
  EXPECT_EQ(expectedSerlized.toJson(), serialized.toJson());

  auto deserialized =
      arangodb::velocypack::deserialize<arangodb::ResultT<uint64_t>>(slice);
  EXPECT_EQ(result, deserialized);
}

TEST_F(VPackInspectionTest, ResultTWithTInside) {
  arangodb::ResultT<uint64_t> result = 45;
  VPackBuilder expectedSerlized;
  {
    VPackObjectBuilder ob(&expectedSerlized);
    expectedSerlized.add("value", 45);
  }

  VPackBuilder serialized;
  arangodb::velocypack::serialize(serialized, result);
  auto slice = serialized.slice();
  EXPECT_EQ(expectedSerlized.toJson(), serialized.toJson());

  auto deserialized =
      arangodb::velocypack::deserialize<arangodb::ResultT<uint64_t>>(slice);
  EXPECT_EQ(result, deserialized);
}

}  // namespace

using namespace arangodb::inspection;
using namespace arangodb::velocypack;

namespace {
struct ErrorTTest {
  std::string s;
  size_t id;
  bool operator==(ErrorTTest const&) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, ErrorTTest& x) {
  return f.object(x).fields(f.field("s", x.s), f.field("id", x.id));
}
}  // namespace

TEST(VPackWithStatus, statust_test_deserialize) {
  auto testSlice = R"({
    "s": "ReturnNode",
    "id": 3
  })"_vpack;

  auto res = deserializeWithErrorT<ErrorTTest>(testSlice);

  ASSERT_TRUE(res.ok()) << fmt::format("Something went wrong: {}",
                                       res.error().error());

  EXPECT_EQ(res->s, "ReturnNode");
  EXPECT_EQ(res->id, 3u);
}

TEST(VPackWithStatus, statust_test_deserialize_fail) {
  auto testSlice = R"({
    "s": "ReturnNode",
    "id": 3,
    "fehler": 2
  })"_vpack;

  auto res = deserializeWithErrorT<ErrorTTest>(testSlice);

  ASSERT_FALSE(res.ok()) << fmt::format("Did not detect the error we exepct");

  EXPECT_EQ(res.error().error(), "Found unexpected attribute 'fehler'");
}
