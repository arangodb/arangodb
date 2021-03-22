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

static std::shared_ptr<Builder> BuildValue(std::string const& value) {
  Parser parser;
  parser.parse(value);
  return parser.steal();
}

TEST(SliceContainerTest, FromUInt8) {
  std::shared_ptr<Builder> builder = BuildValue("null");

  uint8_t* begin = builder->start();
  Slice slice = builder->slice();

  SliceContainer sb(begin, slice.byteSize());
 
  ASSERT_EQ(sb.byteSize(), slice.byteSize());
  ASSERT_EQ(0, memcmp(sb.data(), builder->start(), slice.byteSize()));
  ASSERT_EQ(1UL, sb.byteSize());
  ASSERT_TRUE(sb.slice().isNull());
  ASSERT_NE(sb.data(), begin);
} 

TEST(SliceContainerTest, FromChar) {
  std::shared_ptr<Builder> builder = BuildValue("null");

  uint8_t const* begin = builder->start();
  Slice slice = builder->slice();

  SliceContainer sb(reinterpret_cast<char const*>(begin), slice.byteSize());
  
  ASSERT_EQ(sb.byteSize(), slice.byteSize());
  ASSERT_EQ(0, memcmp(sb.data(), builder->start(), slice.byteSize()));
  ASSERT_EQ(1UL, sb.byteSize());
  ASSERT_TRUE(sb.slice().isNull());
  ASSERT_NE(sb.data(), begin);
} 

TEST(SliceContainerTest, FromUInt8Longer) {
  std::shared_ptr<Builder> builder = BuildValue("[\"the eagle has landed\",\"test\",\"qux\"]");

  uint8_t const* begin = builder->start();
  Slice slice = builder->slice();

  SliceContainer sb(begin, slice.byteSize());
  
  ASSERT_EQ(sb.byteSize(), slice.byteSize());
  ASSERT_EQ(0, memcmp(sb.data(), builder->start(), slice.byteSize()));
  ASSERT_TRUE(sb.slice().isArray());
  ASSERT_NE(sb.data(), begin);
} 

TEST(SliceContainerTest, FromSlice) {
  std::shared_ptr<Builder> builder = BuildValue("\"this is a string of 20 bytes\"");

  Slice slice = builder->slice();
  SliceContainer sb(slice);
  
  ASSERT_EQ(sb.byteSize(), slice.byteSize());
  ASSERT_EQ(0, memcmp(sb.data(), slice.begin(), slice.byteSize()));
  ASSERT_EQ(29UL, sb.byteSize());
  ASSERT_TRUE(sb.slice().isString());
  ASSERT_EQ("this is a string of 20 bytes", sb.slice().copyString());
  ASSERT_NE(sb.data(), slice.begin());
} 

TEST(SliceContainerTest, FromSliceLonger) {
  std::string s("[-1");
  for (std::size_t i = 0; i < 2000; ++i) {   
    s.push_back(',');
    s.append(std::to_string(i));
  } 
  s.push_back(']');

  std::shared_ptr<Builder> builder = BuildValue(s);

  Slice slice = builder->slice();
  SliceContainer sb(slice);
  
  ASSERT_EQ(sb.byteSize(), slice.byteSize());
  ASSERT_EQ(0, memcmp(sb.data(), builder->start(), slice.byteSize()));
  ASSERT_TRUE(sb.slice().isArray());
  ASSERT_NE(sb.data(), slice.begin());
} 

TEST(SliceContainerTest, CopyConstruct) {
  std::shared_ptr<Builder> builder = BuildValue("\"this is a string of 20 bytes\"");

  Slice slice = builder->slice();
  SliceContainer sb(slice.begin(), slice.byteSize());
  SliceContainer sb2(sb);
  
  ASSERT_EQ(sb.byteSize(), sb2.byteSize());
  ASSERT_EQ(0, memcmp(sb.data(), sb2.data(), sb.byteSize()));
  ASSERT_EQ(29UL, sb.byteSize());
  ASSERT_EQ(29UL, sb2.byteSize());
  ASSERT_TRUE(sb.slice().isString());
  ASSERT_EQ("this is a string of 20 bytes", sb.slice().copyString());
  ASSERT_TRUE(sb2.slice().isString());
  ASSERT_EQ("this is a string of 20 bytes", sb2.slice().copyString());
  ASSERT_NE(sb.data(), sb2.data());
} 

TEST(SliceContainerTest, CopyAssign) {
  std::shared_ptr<Builder> builder = BuildValue("\"this is a string of 20 bytes\"");

  Slice slice = builder->slice();
  SliceContainer sb(slice.begin(), slice.byteSize());
  SliceContainer sb2(slice.begin(), slice.byteSize());
  sb2 = sb;
  
  ASSERT_EQ(sb.byteSize(), sb2.byteSize());
  ASSERT_EQ(0, memcmp(sb.data(), sb2.data(), sb.byteSize()));
  ASSERT_EQ(29UL, sb.byteSize());
  ASSERT_EQ(29UL, sb2.byteSize());
  ASSERT_TRUE(sb.slice().isString());
  ASSERT_EQ("this is a string of 20 bytes", sb.slice().copyString());
  ASSERT_TRUE(sb2.slice().isString());
  ASSERT_EQ("this is a string of 20 bytes", sb2.slice().copyString());
  ASSERT_NE(sb.data(), sb2.data());
} 

TEST(SliceContainerTest, MoveConstruct) {
  std::shared_ptr<Builder> builder = BuildValue("\"this is a string of 20 bytes\"");

  Slice slice = builder->slice();
  SliceContainer sb(slice.begin(), slice.byteSize());
  SliceContainer sb2(std::move(sb));
 
  ASSERT_TRUE(sb.slice().isNone()); // must be empty now 
  ASSERT_EQ(1UL, sb.slice().byteSize());

  ASSERT_EQ(29UL, sb2.byteSize());
  ASSERT_TRUE(sb2.slice().isString());
  ASSERT_EQ("this is a string of 20 bytes", sb2.slice().copyString());
  
  ASSERT_NE(sb.slice().begin(), sb2.slice().begin());
} 

TEST(SliceContainerTest, MoveAssign) {
  std::shared_ptr<Builder> builder = BuildValue("\"this is a string of 20 bytes\"");

  Slice slice = builder->slice();
  SliceContainer sb(slice.begin(), slice.byteSize());
  SliceContainer sb2(slice.begin(), slice.byteSize());
  sb2 = std::move(sb);
 
  ASSERT_TRUE(sb.slice().isNone()); // must be empty now 
  ASSERT_EQ(1UL, sb.slice().byteSize());

  ASSERT_EQ(29UL, sb2.byteSize());
  ASSERT_TRUE(sb2.slice().isString());
  ASSERT_EQ("this is a string of 20 bytes", sb2.slice().copyString());

  ASSERT_NE(sb.slice().begin(), sb2.slice().begin());
} 

TEST(SliceContainerTest, SizeLengthByteSize) {
  std::shared_ptr<Builder> builder = BuildValue("\"this is a string of 20 bytes\"");

  Slice slice = builder->slice();
  SliceContainer sb(slice.begin(), slice.byteSize());
 
  ASSERT_TRUE(sb.slice().isString()); 
  ASSERT_EQ(29UL, sb.size());
  ASSERT_EQ(29UL, sb.length());
  ASSERT_EQ(29UL, sb.byteSize());
  ASSERT_EQ(sb.data(), sb.begin());

  Slice empty;
  sb = SliceContainer(empty);
  
  ASSERT_EQ(1UL, sb.size());
  ASSERT_EQ(1UL, sb.length());
  ASSERT_EQ(1UL, sb.byteSize());
  ASSERT_EQ(sb.data(), sb.begin());
} 

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
