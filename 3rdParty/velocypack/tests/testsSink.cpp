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

TEST(SinkTest, CharBufferSink) {
  CharBuffer out;
  CharBufferSink s(&out);

  ASSERT_TRUE(out.empty());
  
  s.push_back('x');
  ASSERT_EQ(1, out.size());
  
  out.clear();
  s.append(std::string("foobarbaz"));
  ASSERT_EQ(9, out.size());

  out.clear();
  s.append("foobarbaz");
  ASSERT_EQ(9, out.size());
  
  out.clear();
  s.append("foobarbaz", 9);
  ASSERT_EQ(9, out.size());
}

TEST(SinkTest, StringSink) {
  std::string out;
  StringSink s(&out);

  ASSERT_TRUE(out.empty());
  
  s.push_back('x');
  ASSERT_EQ(1, out.size());
  ASSERT_EQ("x", out);
  
  out.clear();
  s.append(std::string("foobarbaz"));
  ASSERT_EQ(9, out.size());
  ASSERT_EQ("foobarbaz", out);

  out.clear();
  s.append("foobarbaz");
  ASSERT_EQ(9, out.size());
  ASSERT_EQ("foobarbaz", out);
  
  out.clear();
  s.append("foobarbaz", 9);
  ASSERT_EQ(9, out.size());
  ASSERT_EQ("foobarbaz", out);
}

TEST(SinkTest, StringLengthSink) {
  StringLengthSink s;

  s.push_back('x');
  ASSERT_EQ(1, s.length);
  
  s.append(std::string("foobarbaz"));
  ASSERT_EQ(10, s.length);

  s.append("foobarbaz");
  ASSERT_EQ(19, s.length);
  
  s.append("foobarbaz", 9);
  ASSERT_EQ(28, s.length);
}

TEST(SinkTest, StringStreamSink) {
  std::ostringstream out;
  StringStreamSink s(&out);

  s.push_back('x');
  ASSERT_EQ("x", out.str());
  
  s.append(std::string("foobarbaz"));
  ASSERT_EQ("xfoobarbaz", out.str());

  s.append("foobarbaz");
  ASSERT_EQ("xfoobarbazfoobarbaz", out.str());
  
  s.append("foobarbaz", 9);
  ASSERT_EQ("xfoobarbazfoobarbazfoobarbaz", out.str());
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
