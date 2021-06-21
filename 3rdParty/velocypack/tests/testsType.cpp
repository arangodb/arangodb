////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <ostream>
#include <string>

#include "tests-common.h"

TEST(TypesTest, TestGroups) {
  ASSERT_EQ(ValueType::None, valueTypeGroup(ValueType::None));
  ASSERT_EQ(ValueType::Null, valueTypeGroup(ValueType::Null));
  ASSERT_EQ(ValueType::Bool, valueTypeGroup(ValueType::Bool));
  ASSERT_EQ(ValueType::Double, valueTypeGroup(ValueType::Double));
  ASSERT_EQ(ValueType::String, valueTypeGroup(ValueType::String));
  ASSERT_EQ(ValueType::Array, valueTypeGroup(ValueType::Array));
  ASSERT_EQ(ValueType::Object, valueTypeGroup(ValueType::Object));
  ASSERT_EQ(ValueType::External, valueTypeGroup(ValueType::External));
  ASSERT_EQ(ValueType::UTCDate, valueTypeGroup(ValueType::UTCDate));
  ASSERT_EQ(ValueType::Double, valueTypeGroup(ValueType::Int));
  ASSERT_EQ(ValueType::Double, valueTypeGroup(ValueType::UInt));
  ASSERT_EQ(ValueType::Double, valueTypeGroup(ValueType::SmallInt));
  ASSERT_EQ(ValueType::Binary, valueTypeGroup(ValueType::Binary));
  ASSERT_EQ(ValueType::BCD, valueTypeGroup(ValueType::BCD));
  ASSERT_EQ(ValueType::MinKey, valueTypeGroup(ValueType::MinKey));
  ASSERT_EQ(ValueType::MaxKey, valueTypeGroup(ValueType::MaxKey));
  ASSERT_EQ(ValueType::Custom, valueTypeGroup(ValueType::Custom));
}

TEST(TypesTest, TestNames) {
  ASSERT_STREQ("none", valueTypeName(ValueType::None));
  ASSERT_STREQ("null", valueTypeName(ValueType::Null));
  ASSERT_STREQ("bool", valueTypeName(ValueType::Bool));
  ASSERT_STREQ("double", valueTypeName(ValueType::Double));
  ASSERT_STREQ("string", valueTypeName(ValueType::String));
  ASSERT_STREQ("array", valueTypeName(ValueType::Array));
  ASSERT_STREQ("object", valueTypeName(ValueType::Object));
  ASSERT_STREQ("external", valueTypeName(ValueType::External));
  ASSERT_STREQ("utc-date", valueTypeName(ValueType::UTCDate));
  ASSERT_STREQ("int", valueTypeName(ValueType::Int));
  ASSERT_STREQ("uint", valueTypeName(ValueType::UInt));
  ASSERT_STREQ("smallint", valueTypeName(ValueType::SmallInt));
  ASSERT_STREQ("binary", valueTypeName(ValueType::Binary));
  ASSERT_STREQ("bcd", valueTypeName(ValueType::BCD));
  ASSERT_STREQ("min-key", valueTypeName(ValueType::MinKey));
  ASSERT_STREQ("max-key", valueTypeName(ValueType::MaxKey));
  ASSERT_STREQ("custom", valueTypeName(ValueType::Custom));
}

TEST(TypesTest, TestNamesArrays) {
  uint8_t const arrays[] = {0x01, 0x02, 0x03, 0x04, 0x05,
                            0x06, 0x07, 0x08, 0x9};
  ASSERT_STREQ("array", valueTypeName(Slice(&arrays[0]).type()));
  ASSERT_STREQ("array", valueTypeName(Slice(&arrays[1]).type()));
  ASSERT_STREQ("array", valueTypeName(Slice(&arrays[2]).type()));
  ASSERT_STREQ("array", valueTypeName(Slice(&arrays[3]).type()));
  ASSERT_STREQ("array", valueTypeName(Slice(&arrays[4]).type()));
  ASSERT_STREQ("array", valueTypeName(Slice(&arrays[5]).type()));
  ASSERT_STREQ("array", valueTypeName(Slice(&arrays[6]).type()));
  ASSERT_STREQ("array", valueTypeName(Slice(&arrays[7]).type()));
}

TEST(TypesTest, TestNamesObjects) {
  uint8_t const objects[] = {0x0a, 0x0b, 0x0c, 0x0d, 0x0e};
  ASSERT_STREQ("object", valueTypeName(Slice(&objects[0]).type()));
  ASSERT_STREQ("object", valueTypeName(Slice(&objects[1]).type()));
  ASSERT_STREQ("object", valueTypeName(Slice(&objects[2]).type()));
  ASSERT_STREQ("object", valueTypeName(Slice(&objects[3]).type()));
  ASSERT_STREQ("object", valueTypeName(Slice(&objects[4]).type()));
}

TEST(TypesTest, TestInvalidType) {
  ASSERT_STREQ("unknown", valueTypeName(static_cast<ValueType>(0xff)));
}

TEST(TypesTest, TestStringifyObject) {
  Builder b;
  b.add(Value(ValueType::Object));
  b.close();

  Slice s(b.start());
  std::ostringstream out;
  out << s.type();

  ASSERT_EQ("object", out.str());
}

TEST(TypesTest, TestStringifyArray) {
  Builder b;
  b.add(Value(ValueType::Array));
  b.close();

  Slice s(b.start());
  std::ostringstream out;
  out << s.type();

  ASSERT_EQ("array", out.str());
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
