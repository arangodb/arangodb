////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "VelocypackUtils/VelocyPackStringLiteral.h"

using namespace arangodb::velocypack;

TEST(VelocyPackStringLiteralTest, empty_raw_string) {
  // R"=()="_vpack gives content ")\"" (invalid JSON). Use minimal valid JSON
  // with delimiter '=': empty array R"=([])="_vpack.
  auto t = R"=([])="_vpack;
  EXPECT_FALSE(t.slice().isIllegal());
  EXPECT_TRUE(t.slice().isArray());
  EXPECT_EQ(t.slice().length(), 0u);
  auto buf = toBuffer(t);
  EXPECT_GE(buf.size(), 1u);
}

TEST(VelocyPackStringLiteralTest, empty_array) {
  auto t = R"=([])="_vpack;
  ASSERT_TRUE(t.slice().isArray());
}

TEST(VelocyPackStringLiteralTest, array_one_two_three) {
  auto t = R"=([1,2,3])="_vpack;
  ASSERT_TRUE(t.slice().isArray());
  ASSERT_EQ(t.slice().length(), 3u);
  ASSERT_EQ(t.slice().at(0).getNumber<int64_t>(), 1);
  ASSERT_EQ(t.slice().at(1).getNumber<int64_t>(), 2);
  ASSERT_EQ(t.slice().at(2).getNumber<int64_t>(), 3);
}
