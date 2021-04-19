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

TEST(BuilderTest, FixedArraysSizes) {
  ValueLength const kB = 1024;
  ValueLength const GB = 1024 * 1024 * 1024;
  ValueLength const nrs[] = {1,                    // bytelen < 256
                             2,                    // 256 <= bytelen < 64k
                             (64 * kB) / 127 - 1,  // 256 <= bytelen < 64k
                             (64 * kB) / 127,      // 64k <= bytelen < 4G
                             (4 * GB) / 127,       // 64k <= bytelen < 4G
                             (4 * GB) / 127 + 1};  // 4G <= bytelen
  ValueLength const byteSizes[] = {
      1 + 1 + 1 * 127,                     1 + 8 + 2 * 127,
      1 + 8 + ((64 * kB) / 127 - 1) * 127, 1 + 8 + ((64 * kB) / 127) * 127,
      1 + 8 + ((4 * GB) / 127) * 127,      1 + 8 + ((4 * GB) / 127 + 1) * 127};
  int nr = sizeof(nrs) / sizeof(ValueLength);

  std::string x;
  for (std::size_t i = 0; i < 128 - 2; i++) {
    x.push_back('x');
  }
  // Now x has length 128-2 and thus will use 127 bytes as an entry in an array

  for (int i = 0; i < nr; i++) {
    Builder b;
    b.reserve(byteSizes[i]);
    b.add(Value(ValueType::Array));
    for (ValueLength j = 0; j < nrs[i]; j++) {
      b.add(Value(x));
    }
    b.close();
    uint8_t* start = b.start();

    Slice s(start);
    checkBuild(s, ValueType::Array, byteSizes[i]);
    ASSERT_TRUE(0x02 <= *start && *start <= 0x05);  // Array without index tab
    ASSERT_TRUE(s.isArray());
    ASSERT_EQ(nrs[i], s.length());
    ASSERT_TRUE(s[0].isString());
    ValueLength len;
    char const* p = s[0].getString(len);
    ASSERT_EQ(x.size(), len);
    ASSERT_EQ(x, std::string(p, len));
  }
}

TEST(BuilderTest, ArraysSizes) {
  ValueLength const kB = 1024;
  ValueLength const GB = 1024 * 1024 * 1024;
  ValueLength const nrs[] = {1,                    // bytelen < 256
                             2,                    // 256 <= bytelen < 64k
                             (64 * kB) / 129 - 1,  // 256 <= bytelen < 64k
                             (64 * kB) / 129,      // 64k <= bytelen < 4G
                             (4 * GB) / 131,       // 64k <= bytelen < 4G
                             (4 * GB) / 131 + 1};  // 4G <= bytelen
  ValueLength const byteSizes[] = {1 + 1 + 1 + 2 + 1 * 128,
                                   1 + 8 + 3 + 2 * 129,
                                   1 + 8 + 3 + ((64 * kB) / 129 - 1) * 129,
                                   1 + 8 + 5 + ((64 * kB) / 129) * 131,
                                   1 + 8 + 5 + ((4 * GB) / 131) * 131,
                                   1 + 8 + 9 + ((4 * GB) / 131 + 1) * 135 + 8};
  int nr = sizeof(nrs) / sizeof(ValueLength);

  std::string x;
  for (std::size_t i = 0; i < 128 - 2; i++) {
    x.push_back('x');
  }
  // Now x has length 128-2 and thus will use 127 bytes as an entry in an array

  for (int i = 0; i < nr; i++) {
    Builder b;
    b.reserve(byteSizes[i]);
    b.add(Value(ValueType::Array));
    b.add(Value(1));
    for (ValueLength j = 0; j < nrs[i]; j++) {
      b.add(Value(x));
    }
    b.close();
    uint8_t* start = b.start();

    Slice s(start);
    checkBuild(s, ValueType::Array, byteSizes[i]);
    ASSERT_TRUE(0x06 <= *start && *start <= 0x09);  // Array without index tab
    ASSERT_TRUE(s.isArray());
    ASSERT_EQ(nrs[i] + 1, s.length());
    ASSERT_TRUE(s[0].isSmallInt());
    ASSERT_EQ(1LL, s[0].getInt());
    ASSERT_TRUE(s[1].isString());
    ValueLength len;
    char const* p = s[1].getString(len);
    ASSERT_EQ(x.size(), len);
    ASSERT_EQ(x, std::string(p, len));
  }
}

TEST(BuilderTest, ObjectsSizesSorted) {
  ValueLength const kB = 1024;
  ValueLength const GB = 1024 * 1024 * 1024;
  ValueLength const nrs[] = {1,                    // bytelen < 256
                             2,                    // 256 <= bytelen < 64k
                             (64 * kB) / 130,      // 256 <= bytelen < 64k
                             (64 * kB) / 130 + 1,  // 64k <= bytelen < 4G
                             (4 * GB) / 132 - 1,   // 64k <= bytelen < 4G
                             (4 * GB) / 132};      // 4G <= bytelen
  ValueLength const byteSizes[] = {
      1 + 1 + 1 + 1 + 1 * 128,            1 + 8 + 2 * 130,
      1 + 8 + ((64 * kB) / 130) * 130,    1 + 8 + ((64 * kB) / 130 + 1) * 132,
      1 + 8 + ((4 * GB) / 132 - 1) * 132, 1 + 8 + ((4 * GB) / 132) * 136 + 8};
  int nr = sizeof(nrs) / sizeof(ValueLength);

  std::string x;
  for (std::size_t i = 0; i < 118 - 1; i++) {
    x.push_back('x');
  }
  // Now x has length 118-1 and thus will use 118 bytes as an entry in an object
  // The attribute name generated below will use another 10.

  for (int i = 0; i < nr; i++) {
    Builder b;
    b.reserve(byteSizes[i]);
    b.add(Value(ValueType::Object));
    for (ValueLength j = 0; j < nrs[i]; j++) {
      std::string attrName = "axxxxxxxx";
      ValueLength n = j;
      for (int k = 8; k >= 1; k--) {
        attrName[k] = (n % 26) + 'A';
        n /= 26;
      }
      b.add(attrName, Value(x));
    }
    b.close();
    uint8_t* start = b.start();

    Slice s(start);
    checkBuild(s, ValueType::Object, byteSizes[i]);
    if (nrs[i] == 1) {
      ASSERT_TRUE(*start = 0x14);
    } else {
      ASSERT_TRUE(0x0b <= *start && *start <= 0x0e);  // Object
    }
    ASSERT_TRUE(s.isObject());
    ASSERT_EQ(nrs[i], s.length());
    ASSERT_TRUE(s["aAAAAAAAA"].isString());
    ValueLength len;
    char const* p = s["aAAAAAAAA"].getString(len);
    ASSERT_EQ(x.size(), len);
    ASSERT_EQ(x, std::string(p, len));
  }
}

TEST(ParserTest, EmptyAttributeName) {
  Builder builder;
  Parser parser(builder);
  parser.parse(R"({"":123,"a":"abc"})");
  Slice s = builder.slice();
  
  ASSERT_EQ(2UL, s.length());

  Slice ss = s.get("");
  ASSERT_FALSE(ss.isNone());
  ASSERT_TRUE(ss.isInteger());
  ASSERT_EQ(123L, ss.getInt());

  ss = s.get("a");
  ASSERT_FALSE(ss.isNone());
  ASSERT_TRUE(ss.isString());
  ASSERT_EQ(std::string("abc"), ss.copyString());

  ss = s.get("b");
  ASSERT_TRUE(ss.isNone());
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
