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
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>

#include "Basics/VelocyPackHelper.h"
#include "VelocypackUtils/VelocyPackStringLiteral.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

#define VPACK_EXPECT_TRUE(expected, func, lValue, rValue) \
  l = VPackParser::fromJson(lValue);                      \
  r = VPackParser::fromJson(rValue);                      \
  EXPECT_EQ(expected, func(l->slice(), r->slice(), true));

struct DoubleValue {
  double d;
  uint8_t sign;
  uint16_t E;
  uint64_t M;

  std::string to_string() {
    return std::to_string(sign) + " " + std::to_string(E) + " " +
           std::to_string(M);
  }
};

DoubleValue makeDoubleValue(uint8_t sign, uint16_t E, uint64_t M) {
  EXPECT_LT(sign, 2);
  EXPECT_LT(E, 2048);
  EXPECT_LT(M, uint64_t{1} << 52);
  uint64_t x = (static_cast<uint64_t>(sign) << 63) |
               (static_cast<uint64_t>(E) << 52) | static_cast<uint64_t>(M);
  double y;
  memcpy(&y, &x, 8);
  return DoubleValue{.d = y, .sign = sign, .E = E, .M = M};
}

template<typename T>
VPackBuilder makeVPack(T x) {
  VPackBuilder b;
  b.add(VPackValue(x));
  return b;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test compare values with equal values
////////////////////////////////////////////////////////////////////////////////

TEST(VPackHelperTest, tst_compare_values_equal) {
  std::shared_ptr<VPackBuilder> l;
  std::shared_ptr<VPackBuilder> r;

  // With Utf8-mode:
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "null",
                    "null");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "false",
                    "false");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "true",
                    "true");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "0", "0");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "1", "1");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "1.5",
                    "1.5");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "-43.2",
                    "-43.2");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "\"\"",
                    "\"\"");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "\" \"",
                    "\" \"");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare,
                    "\"the quick brown fox\"", "\"the quick brown fox\"");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "[]", "[]");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "[-1]",
                    "[-1]");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "[0]",
                    "[0]");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "[1]",
                    "[1]");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "[true]",
                    "[true]");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "{}", "{}");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test compare values with unequal values
////////////////////////////////////////////////////////////////////////////////

TEST(VPackHelperTest, tst_compare_values_unequal) {
  std::shared_ptr<VPackBuilder> l;
  std::shared_ptr<VPackBuilder> r;
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null",
                    "false");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null",
                    "true");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null",
                    "-1");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null",
                    "0");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null",
                    "1");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null",
                    "-10");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null",
                    "\"\"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null",
                    "\"0\"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null",
                    "\" \"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null",
                    "[]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null",
                    "[null]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null",
                    "[false]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null",
                    "[true]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null",
                    "[0]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null",
                    "{}");

  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false",
                    "true");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false",
                    "-1");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false",
                    "0");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false",
                    "1");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false",
                    "-10");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false",
                    "\"\"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false",
                    "\"0\"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false",
                    "\" \"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false",
                    "[]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false",
                    "[null]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false",
                    "[false]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false",
                    "[true]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false",
                    "[0]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false",
                    "{}");

  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true",
                    "-1");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true",
                    "0");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true",
                    "1");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true",
                    "-10");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true",
                    "\"\"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true",
                    "\"0\"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true",
                    "\" \"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true",
                    "[]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true",
                    "[null]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true",
                    "[false]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true",
                    "[true]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true",
                    "[0]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true",
                    "{}");

  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "-2",
                    "-1");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "-10",
                    "-9");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "-20",
                    "-5");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "-5",
                    "-2");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true",
                    "1");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1.5",
                    "1.6");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "10.5",
                    "10.51");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0",
                    "\"\"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0",
                    "\"0\"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0",
                    "\"-1\"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1",
                    "\"-1\"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1",
                    "\" \"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0",
                    "[-1]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0",
                    "[0]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0",
                    "[1]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0",
                    "[null]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0",
                    "[false]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0",
                    "[true]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0", "{}");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1",
                    "[-1]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1",
                    "[0]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1",
                    "[1]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1",
                    "[null]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1",
                    "[false]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1",
                    "[true]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1", "{}");
}

using namespace arangodb::velocypack;

TEST(VPackHelperTest, velocypack_string_literals) {
  {
    auto const& s = "4"_vpack;
    ASSERT_EQ(s.getUInt(), 4);
  }

  {
    auto const& array = R"([1,2,3,4])"_vpack;
    ASSERT_EQ(array.slice()[0].getUInt(), 1);
    ASSERT_EQ(array.slice()[1].getUInt(), 2);
    ASSERT_EQ(array.slice()[2].getUInt(), 3);
    ASSERT_EQ(array.slice()[3].getUInt(), 4);
  }

  {
    auto const& obj = R"({
                 "vertices": [ {"_key" : "A"}, {"_key" : "B"}, {"_key" : "C"} ],
                 "edges": [ {"_from" : "A", "_to" : "B"},
                            {"_from" : "B", "_to" : "C"} ]
    })"_vpack;
    ASSERT_TRUE(obj.slice()["vertices"].isArray());
    ASSERT_TRUE(obj.slice()["edges"].isArray());
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief test comparison of numerical values
//////////////////////////////////////////////////////////////////////////////

constexpr uint64_t mantmax = (uint64_t(1) << 52) - 1;

using VelocyPackHelper = arangodb::basics::VelocyPackHelper;

TEST(VPackHelperTest, test_comparison_numerical) {
  // We create a vector of numerical velocypack values which is supposed
  // to be sorted stricly ascending. We also check transitivity by comparing
  // all pairs:
  std::vector<VPackBuilder> v;
  v.push_back(makeVPack(makeDoubleValue(1, 2047, 0).d));  // -Inf
  for (uint16_t i = 2046; i >= 2; --i) {
    v.push_back(makeVPack(makeDoubleValue(1, i, mantmax).d));
    v.push_back(makeVPack(makeDoubleValue(1, i, 0).d));
  }
  v.push_back(makeVPack(makeDoubleValue(1, 1, mantmax).d));  // -2.0+2^-52
  v.push_back(makeVPack(makeDoubleValue(1, 1, 0).d));        // - 1.0
  v.push_back(makeVPack(makeDoubleValue(1, 0, mantmax).d));  // - denormalized
  v.push_back(makeVPack(makeDoubleValue(1, 0, 1).d));        // - denormalized
  // v.push_back(makeVPack(makeDoubleValue(1, 0, 0).d));        // - 0

  v.push_back(makeVPack(makeDoubleValue(0, 0, 0).d));        // + 0
  v.push_back(makeVPack(makeDoubleValue(0, 0, 1).d));        // + denormalized
  v.push_back(makeVPack(makeDoubleValue(0, 0, mantmax).d));  // + denormalized
  v.push_back(makeVPack(makeDoubleValue(0, 1, 0).d));        // + 1.0
  v.push_back(makeVPack(makeDoubleValue(0, 1, mantmax).d));  // + 2.0-2^-52
  for (uint16_t i = 2; i <= 2046; ++i) {
    v.push_back(makeVPack(makeDoubleValue(0, i, 0).d));
    v.push_back(makeVPack(makeDoubleValue(0, i, mantmax).d));
  }
  v.push_back(makeVPack(makeDoubleValue(0, 2047, 0).d));  // infinity

  // Now check if our comparator agrees that this is strictly ascending:
  for (std::size_t i = 0; i < v.size() - 1; ++i) {
    EXPECT_EQ(-1, VelocyPackHelper::compareNumberValuesCorrectly(
                      v[i].slice().type(), v[i].slice(), v[i + 1].slice()));
  }
}

TEST(VPackHelperTest, test_equality_zeros) {
  std::vector<VPackBuilder> v;
  // +0.0:
  v.push_back(makeVPack(makeDoubleValue(0, 0, 0).d));
  // -0.0:
  v.push_back(makeVPack(makeDoubleValue(1, 0, 0).d));
  // uint64_t{0}:
  v.push_back(makeVPack(VPackValue(uint64_t{0})));
  // int64_t{0}:
  v.push_back(makeVPack(VPackValue(int64_t{0})));
  // smallint{0}:
  v.push_back(makeVPack(VPackValue(0, VPackValueType::SmallInt)));
  for (std::size_t i = 0; i < v.size(); ++i) {
    for (std::size_t j = 0; j < v.size(); ++j) {
      EXPECT_EQ(0, VelocyPackHelper::compareNumberValuesCorrectly(
                       v[i].slice().type(), v[i].slice(), v[j].slice()));
    }
  }
}
