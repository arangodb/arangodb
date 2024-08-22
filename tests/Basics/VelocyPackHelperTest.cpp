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

static DoubleValue makeDoubleValue(uint8_t sign, uint16_t E, uint64_t M) {
  EXPECT_LT(sign, 2);
  EXPECT_LT(E, 2048);
  EXPECT_LT(M, uint64_t{1} << 52);
  uint64_t x = (static_cast<uint64_t>(sign) << 63) |
               (static_cast<uint64_t>(E) << 52) | static_cast<uint64_t>(M);
  double y = std::bit_cast<double>(x);
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

TEST(VPackHelperTest, test_comparison_numerical_double) {
  for (int n = 0; n < 2; ++n) {
    // Do everything twice, once with +0 and once with -0

    // We create a vector of numerical velocypack values which is supposed
    // to be sorted strictly ascending. We also check transitivity by comparing
    // all pairs:
    std::vector<VPackBuilder> v;
    v.push_back(makeVPack(makeDoubleValue(1, 2047, 0).d));  // -Inf
    for (uint16_t i = 2046; i >= 1; --i) {
      v.push_back(makeVPack(makeDoubleValue(1, i, mantmax).d));
      v.push_back(makeVPack(makeDoubleValue(1, i, 0).d));
    }
    v.push_back(makeVPack(makeDoubleValue(1, 0, mantmax).d));  // - denormalized
    v.push_back(makeVPack(makeDoubleValue(1, 0, 1).d));        // - denormalized
    if (n == 0) {
      v.push_back(makeVPack(makeDoubleValue(0, 0, 0).d));  // + 0
    } else {
      v.push_back(makeVPack(makeDoubleValue(1, 0, 0).d));  // - 0
    }
    v.push_back(makeVPack(makeDoubleValue(0, 0, 1).d));        // + denormalized
    v.push_back(makeVPack(makeDoubleValue(0, 0, mantmax).d));  // + denormalized
    for (uint16_t i = 1; i <= 2046; ++i) {
      v.push_back(makeVPack(makeDoubleValue(0, i, 0).d));
      v.push_back(makeVPack(makeDoubleValue(0, i, mantmax).d));
    }
    v.push_back(makeVPack(makeDoubleValue(0, 2047, 0).d));  // infinity

    // Now check if our comparator agrees that this is strictly ascending:
    for (std::size_t i = 0; i < v.size() - 1; ++i) {
      auto c = VelocyPackHelper::compareNumberValuesCorrectly(
          v[i].slice().type(), v[i].slice(), v[i + 1].slice());
      EXPECT_EQ(-1, c) << "Not strictly increasing: " << i << " "
                       << v[i].slice().toJson() << " "
                       << v[i + 1].slice().toJson();
      c = VelocyPackHelper::compareNumberValuesCorrectly(
          v[i + 1].slice().type(), v[i + 1].slice(), v[i].slice());
      EXPECT_EQ(1, c) << "Not strictly decreasing: " << i << " "
                      << v[i + 1].slice().toJson() << " "
                      << v[i].slice().toJson();
    }
    // Check reflexivity:
    for (std::size_t i = 0; i < v.size(); ++i) {
      auto c = VelocyPackHelper::compareNumberValuesCorrectly(
          v[i].slice().type(), v[i].slice(), v[i].slice());
      EXPECT_EQ(0, c) << "Not reflexive: " << i << " " << v[i].slice().toJson();
    }
    // And check transitivity by comparing all pairs:
    for (std::size_t i = 0; i < v.size() - 1; ++i) {
      for (std::size_t j = i + 1; j < v.size(); ++j) {
        auto c = VelocyPackHelper::compareNumberValuesCorrectly(
            v[i].slice().type(), v[i].slice(), v[j].slice());
        EXPECT_EQ(-1, c) << "Not transitive: " << i << " "
                         << v[i].slice().toJson() << " " << j << " "
                         << v[j].slice().toJson();
      }
    }
    // And the same the other way round
    for (std::size_t i = 0; i < v.size() - 1; ++i) {
      for (std::size_t j = i + 1; j < v.size(); ++j) {
        auto c = VelocyPackHelper::compareNumberValuesCorrectly(
            v[j].slice().type(), v[j].slice(), v[i].slice());
        EXPECT_EQ(1, c) << "Not transitive: " << i << " "
                        << v[i].slice().toJson() << " " << j << " "
                        << v[j].slice().toJson();
      }
    }
  }
}

TEST(VPackHelperTest, test_equality_zeros) {
  std::vector<VPackBuilder> v;
  // +0.0:
  v.push_back(makeVPack(makeDoubleValue(0, 0, 0).d));
  // -0.0:
  v.push_back(makeVPack(makeDoubleValue(1, 0, 0).d));
  // uint64_t{0}:
  v.push_back(makeVPack(uint64_t{0}));
  // int64_t{0}:
  v.push_back(makeVPack(int64_t{0}));
  // smallint{0}:
  v.push_back(makeVPack(VPackValue(0, VPackValueType::SmallInt)));
  for (std::size_t i = 0; i < v.size(); ++i) {
    for (std::size_t j = 0; j < v.size(); ++j) {
      EXPECT_EQ(0, VelocyPackHelper::compareNumberValuesCorrectly(
                       v[i].slice().type(), v[i].slice(), v[j].slice()));
    }
  }
}

TEST(VPackHelperTest, test_equality_with_integers) {
  std::vector<int64_t> vi;
  std::vector<uint64_t> vu;
  vi.push_back(0);
  vi.push_back(0);
  int64_t x = -1;
  uint64_t y = 1;
  for (int i = 0; i < 62; ++i) {
    vi.push_back(x);
    vu.push_back(y);
    x <<= 1;
    y <<= 1;
  }
  for (int64_t i : vi) {
    VPackBuilder l = makeVPack(i);
    VPackBuilder r = makeVPack(static_cast<double>(i));
    EXPECT_EQ(0, VelocyPackHelper::compareNumberValuesCorrectly(
                     l.slice().type(), l.slice(), r.slice()));
    EXPECT_EQ(0, VelocyPackHelper::compareNumberValuesCorrectly(
                     r.slice().type(), r.slice(), l.slice()));
  }
  for (uint64_t u : vu) {
    VPackBuilder l = makeVPack(u);
    VPackBuilder r = makeVPack(static_cast<double>(u));
    EXPECT_EQ(0, VelocyPackHelper::compareNumberValuesCorrectly(
                     l.slice().type(), l.slice(), r.slice()));
    EXPECT_EQ(0, VelocyPackHelper::compareNumberValuesCorrectly(
                     r.slice().type(), r.slice(), l.slice()));
  }
}

TEST(VPackHelperTest, test_inequality_with_integers) {
  int64_t x = -2;
  uint64_t y = 2;
  for (int i = 0; i < 61; ++i) {
    VPackBuilder l = makeVPack(static_cast<double>(x));
    VPackBuilder r = makeVPack(x - 1);
    EXPECT_EQ(1, VelocyPackHelper::compareNumberValuesCorrectly(
                     l.slice().type(), l.slice(), r.slice()))
        << "Not less: " << i << " " << l.slice().toJson() << " "
        << r.slice().toJson();
    EXPECT_EQ(-1, VelocyPackHelper::compareNumberValuesCorrectly(
                      r.slice().type(), r.slice(), l.slice()))
        << "Not greater: " << i << " " << r.slice().toJson() << " "
        << l.slice().toJson();
    VPackBuilder ll = makeVPack(y + 1);
    VPackBuilder rr = makeVPack(static_cast<double>(y));
    EXPECT_EQ(1, VelocyPackHelper::compareNumberValuesCorrectly(
                     ll.slice().type(), ll.slice(), rr.slice()))
        << "Not less: " << i << " " << ll.slice().toJson() << " "
        << rr.slice().toJson();
    EXPECT_EQ(-1, VelocyPackHelper::compareNumberValuesCorrectly(
                      rr.slice().type(), rr.slice(), ll.slice()))
        << "Not greater: " << i << " " << rr.slice().toJson() << " "
        << ll.slice().toJson();
    x <<= 1;
    y <<= 1;
  }
}

TEST(VPackHelperTest, test_numbers_compare_as_doubles) {
  VPackBuilder a = makeVPack(std::numeric_limits<int64_t>::max());

  uint64_t v = std::numeric_limits<int64_t>::max();
  VPackBuilder b = makeVPack(v);

  uint64_t w = v + 1;
  VPackBuilder c = makeVPack(w);

  EXPECT_EQ(0, VelocyPackHelper::compareNumberValuesCorrectly(
                   a.slice().type(), a.slice(), b.slice()));
  EXPECT_EQ(-1, VelocyPackHelper::compareNumberValuesCorrectly(
                    b.slice().type(), b.slice(), c.slice()));
  EXPECT_EQ(-1, VelocyPackHelper::compareNumberValuesCorrectly(
                    a.slice().type(), a.slice(), c.slice()));
}

template<typename T>
static inline void checkNaN(T t) {
  VPackBuilder nan = makeVPack(makeDoubleValue(0, 2047, 1).d);  // NaN
  VPackBuilder a = makeVPack(t);
  EXPECT_EQ(-1, VelocyPackHelper::compareNumberValuesCorrectly(
                    (a).slice().type(), (a).slice(), nan.slice()));
  EXPECT_EQ(1, VelocyPackHelper::compareNumberValuesCorrectly(
                   nan.slice().type(), nan.slice(), (a).slice()));
}

TEST(VPackHelperTest, test_nan_greater_than_all) {
  VPackBuilder a;
  checkNaN(int64_t{0});
  checkNaN(uint64_t{0});
  checkNaN(int64_t{-1});
  checkNaN(int64_t{1});
  checkNaN(uint64_t{1});
  checkNaN(std::numeric_limits<int64_t>::max());
  checkNaN(std::numeric_limits<int64_t>::min());
  checkNaN(std::numeric_limits<uint64_t>::max());
  checkNaN(int64_t{12321222123});
  checkNaN(int64_t{-12321222123});
  checkNaN(uint64_t{12321222123});

  checkNaN(double{0.0});                    // +0
  checkNaN(makeDoubleValue(1, 0, 0).d);     // -0
  checkNaN(makeDoubleValue(0, 2047, 0).d);  // +infty
  checkNaN(makeDoubleValue(1, 2047, 0).d);  // -infty
  checkNaN(double{1.0});
  checkNaN(double{-1.0});
  checkNaN(double{123456.789});
  checkNaN(double{-123456.789});
  checkNaN(double{1.23456e89});
  checkNaN(double{-1.23456e89});
  checkNaN(double{1.23456e-89});
  checkNaN(double{-1.23456e-89});
  checkNaN(makeDoubleValue(0, 0, 1).d);                        // denormalized
  checkNaN(makeDoubleValue(0, 0, 123456789).d);                // denormalized
  checkNaN(makeDoubleValue(0, 0, (uint64_t{1} << 52) - 1).d);  // denormalized
  checkNaN(makeDoubleValue(1, 0, 1).d);                        // denormalized
  checkNaN(makeDoubleValue(1, 0, 123456789).d);                // denormalized
  checkNaN(makeDoubleValue(1, 0, (uint64_t{1} << 52) - 1).d);  // denormalized
}

template<typename A, typename B>
static inline int comp(A a, B b) {
  VPackBuilder Ba = makeVPack(a);
  VPackBuilder Bb = makeVPack(b);
  return VelocyPackHelper::compareNumberValuesCorrectly(Ba.slice().type(),
                                                        Ba.slice(), Bb.slice());
}

TEST(VPackHelperTest, test_unsigned_double_comparison) {
  // Test a large representable value:
  double d = ldexp(1.0, 52);
  uint64_t u = uint64_t{1} << 52;
  EXPECT_EQ(0, comp(d, u));
  EXPECT_EQ(0, comp(u, d));
  EXPECT_EQ(0, comp(d + 1.0, u + 1));
  EXPECT_EQ(0, comp(u + 1, d + 1.0));

  // Test a large non-representable value:
  d = ldexp(1.0, 53);
  u = uint64_t{1} << 53;
  EXPECT_EQ(0, comp(d, u));
  EXPECT_EQ(0, comp(u, d));
  // d+1.0 is equal to d here due to limited precision!
  EXPECT_EQ(-1, comp(d + 1.0, u + 1));
  EXPECT_EQ(1, comp(u + 1, d + 1.0));

  // Test another large non-representable value:
  d = ldexp(1.0, 60);
  u = uint64_t{1} << 60;
  EXPECT_EQ(0, comp(d, u));
  EXPECT_EQ(0, comp(u, d));
  // d+1.0 is equal to d here due to limited precision!
  EXPECT_EQ(-1, comp(d + 1.0, u + 1));
  EXPECT_EQ(1, comp(u + 1, d + 1.0));

  // Test close to the top:
  d = ldexp(1.0, 63);
  u = uint64_t{1} << 63;
  EXPECT_EQ(0, comp(d, u));
  EXPECT_EQ(0, comp(u, d));
  // d+1.0 is equal to d here due to limited precision!
  EXPECT_EQ(-1, comp(d + 1.0, u + 1));
  EXPECT_EQ(1, comp(u + 1, d + 1.0));

  // Test rounding down:
  d = ldexp(1.0, 60);
  u = (uint64_t{1} << 61) - 1;
  EXPECT_EQ(-1, comp(d, u));
  EXPECT_EQ(1, comp(u, d));
  d = ldexp(1.0, 61);
  EXPECT_EQ(1, comp(d, u));
  EXPECT_EQ(-1, comp(u, d));

  // Test doubles between two representable integers:
  d = ldexp(1.0, 51) + 0.5;
  u = uint64_t{1} << 51;
  EXPECT_EQ(1, comp(d, u));
  EXPECT_EQ(-1, comp(u, d));
  EXPECT_EQ(-1, comp(d, u + 1));
  EXPECT_EQ(1, comp(u + 1, d));

  // Test when no precision is lost by a large margin:
  d = 123456789.0;
  u = 123456789;
  EXPECT_EQ(0, comp(d, u));
  EXPECT_EQ(0, comp(u, d));
  EXPECT_EQ(1, comp(d + 0.5, u));
  EXPECT_EQ(-1, comp(u, d + 0.5));
  EXPECT_EQ(1, comp(d + 1.0, u));
  EXPECT_EQ(-1, comp(u, d + 1.0));
  EXPECT_EQ(1, comp(d, u - 1));
  EXPECT_EQ(-1, comp(u - 1, d));
}

TEST(VPackHelperTest, test_signed_double_comparison) {
  // Test a large representable value:
  double d = -ldexp(1.0, 52);
  int64_t i = -(int64_t{1} << 52);
  EXPECT_EQ(0, comp(d, i));
  EXPECT_EQ(0, comp(i, d));
  EXPECT_EQ(0, comp(d + 1.0, i + 1));
  EXPECT_EQ(0, comp(i + 1, d + 1.0));

  // Test a large non-representable value:
  d = -ldexp(1.0, 53);
  i = -(int64_t{1} << 53);
  EXPECT_EQ(0, comp(d, i));
  EXPECT_EQ(0, comp(i, d));
  // d-1.0 is equal to d here due to limited precision!
  EXPECT_EQ(1, comp(d - 1.0, i - 1));
  EXPECT_EQ(-1, comp(i - 1, d - 1.0));

  // Test another large non-representable value:
  d = -ldexp(1.0, 60);
  i = -(int64_t{1} << 60);
  EXPECT_EQ(0, comp(d, i));
  EXPECT_EQ(0, comp(i, d));
  // d+1.0 is equal to d here due to limited precision!
  EXPECT_EQ(-1, comp(d + 1.0, i + 1));
  EXPECT_EQ(1, comp(i + 1, d + 1.0));

  // Test close to the top:
  d = -ldexp(1.0, 62);
  i = -(int64_t{1} << 62);
  EXPECT_EQ(0, comp(d, i));
  EXPECT_EQ(0, comp(i, d));
  // d+1.0 is equal to d here due to limited precision!
  EXPECT_EQ(-1, comp(d + 1.0, i + 1));
  EXPECT_EQ(1, comp(i + 1, d + 1.0));

  // Test rounding down:
  d = -ldexp(1.0, 60);
  i = -((int64_t{1} << 61) - 1);
  EXPECT_EQ(1, comp(d, i));
  EXPECT_EQ(-1, comp(i, d));
  d = -ldexp(1.0, 61);
  EXPECT_EQ(-1, comp(d, i));
  EXPECT_EQ(1, comp(i, d));

  // Test doubles between two representable integers:
  d = -ldexp(1.0, 51) + 0.5;
  i = -(int64_t{1} << 51);
  EXPECT_EQ(1, comp(d, i));
  EXPECT_EQ(-1, comp(i, d));
  EXPECT_EQ(-1, comp(d, i + 1));
  EXPECT_EQ(1, comp(i + 1, d));

  // Test when no precision is lost by a large margin:
  d = -123456789.0;
  i = -123456789;
  EXPECT_EQ(0, comp(d, i));
  EXPECT_EQ(0, comp(i, d));
  EXPECT_EQ(1, comp(d + 0.5, i));
  EXPECT_EQ(-1, comp(i, d + 0.5));
  EXPECT_EQ(1, comp(d + 1.0, i));
  EXPECT_EQ(-1, comp(i, d + 1.0));
  EXPECT_EQ(1, comp(d, i - 1));
  EXPECT_EQ(-1, comp(i - 1, d));

  // Test the smallest signed integer:
  i = std::numeric_limits<int64_t>::min();
  d = -ldexp(1.0, 63);
  EXPECT_EQ(0, comp(d, i));
  EXPECT_EQ(0, comp(i, d));
  EXPECT_EQ(-1, comp(d, i + 1));
  EXPECT_EQ(1, comp(i + 1, d));
}

template<typename A, typename B>
static inline int compGeneric(A a, B b) {
  VPackBuilder Ba = makeVPack(a);
  VPackBuilder Bb = makeVPack(b);
  return VelocyPackHelper::compare(Ba.slice(), Bb.slice(), true);
}

TEST(VPackHelperTest, test_generic_uses_correct_numerical_comparison) {
  // Test large non-representable value:
  double d = ldexp(1.0, 60);
  uint64_t u = uint64_t{1} << 60;
  EXPECT_EQ(0, compGeneric(d, u));
  EXPECT_EQ(0, compGeneric(u, d));
  // d+1.0 is equal to d here due to limited precision!
  EXPECT_EQ(-1, compGeneric(d + 1.0, u + 1));
  EXPECT_EQ(1, compGeneric(u + 1, d + 1.0));

  // Test another large non-representable value:
  d = -ldexp(1.0, 60);
  int64_t i = -(int64_t{1} << 60);
  EXPECT_EQ(0, compGeneric(d, i));
  EXPECT_EQ(0, compGeneric(i, d));
  // d+1.0 is equal to d here due to limited precision!
  EXPECT_EQ(-1, compGeneric(d + 1.0, i + 1));
  EXPECT_EQ(1, compGeneric(i + 1, d + 1.0));

  // Now compare signed and unsigned:
  u = uint64_t{1} << 60;
  i = int64_t{1} << 60;
  EXPECT_EQ(0, compGeneric(u, i));
  EXPECT_EQ(0, compGeneric(i, u));
  EXPECT_EQ(0, compGeneric(u + 1, i + 1));
  EXPECT_EQ(0, compGeneric(i + 1, u + 1));
  EXPECT_EQ(0, compGeneric(u - 1, i - 1));
  EXPECT_EQ(0, compGeneric(i - 1, u - 1));
  EXPECT_EQ(1, compGeneric(u + 1, i));
  EXPECT_EQ(-1, compGeneric(i, u + 1));
  EXPECT_EQ(-1, compGeneric(u - 1, i));
  EXPECT_EQ(1, compGeneric(i, u - 1));
  EXPECT_EQ(1, compGeneric(i + 1, u));
  EXPECT_EQ(-1, compGeneric(u, i + 1));
  EXPECT_EQ(-1, compGeneric(i - 1, u));
  EXPECT_EQ(1, compGeneric(u, i - 1));
}

// So far we do not actually use UTCDate, but for the sake of completeness
// we check if things work as expected. UTCDates are larger than any number
// (including NaN), and smaller than any string:

TEST(VPackHelperTest, test_utcdate) {
  double d = ldexp(1.0, 60);
  uint64_t u = uint64_t{1} << 60;
  int64_t i = int64_t{1} << 60;
  std::string s = "abc";
  EXPECT_EQ(-1, compGeneric(d, VPackValue(i, VPackValueType::UTCDate)));
  EXPECT_EQ(1, compGeneric(VPackValue(i, VPackValueType::UTCDate), d));
  EXPECT_EQ(-1, compGeneric(u, VPackValue(i, VPackValueType::UTCDate)));
  EXPECT_EQ(1, compGeneric(VPackValue(i, VPackValueType::UTCDate), u));
  EXPECT_EQ(-1, compGeneric(i, VPackValue(i, VPackValueType::UTCDate)));
  EXPECT_EQ(1, compGeneric(VPackValue(i, VPackValueType::UTCDate), i));
  EXPECT_EQ(-1, compGeneric(d + 1.0, VPackValue(i, VPackValueType::UTCDate)));
  EXPECT_EQ(1, compGeneric(VPackValue(i + 1, VPackValueType::UTCDate), d));
  EXPECT_EQ(1, compGeneric(VPackValue(i - 1, VPackValueType::UTCDate), d));
  EXPECT_EQ(-1, compGeneric(u + 1, VPackValue(i, VPackValueType::UTCDate)));
  EXPECT_EQ(-1, compGeneric(u - 1, VPackValue(i, VPackValueType::UTCDate)));
  EXPECT_EQ(1, compGeneric(VPackValue(i + 1, VPackValueType::UTCDate), u));
  EXPECT_EQ(1, compGeneric(VPackValue(i - 1, VPackValueType::UTCDate), u));
  EXPECT_EQ(-1, compGeneric(i + 1, VPackValue(i, VPackValueType::UTCDate)));
  EXPECT_EQ(-1, compGeneric(i - 1, VPackValue(i, VPackValueType::UTCDate)));
  EXPECT_EQ(1, compGeneric(VPackValue(i + 1, VPackValueType::UTCDate), i));
  EXPECT_EQ(1, compGeneric(VPackValue(i - 1, VPackValueType::UTCDate), i));
  EXPECT_EQ(-1,
            compGeneric(VPackValue(i, VPackValueType::UTCDate), VPackValue(s)));
  EXPECT_EQ(1,
            compGeneric(VPackValue(s), VPackValue(i, VPackValueType::UTCDate)));
}

TEST(VPackHelperTest, test_small_int_with_int) {
  int64_t i = int64_t{5};
  EXPECT_EQ(0, compGeneric(i, VPackValue(i, VPackValueType::SmallInt)));
  EXPECT_EQ(0, compGeneric(VPackValue(i, VPackValueType::SmallInt), i));
  EXPECT_EQ(1, compGeneric(i + 1, VPackValue(i, VPackValueType::SmallInt)));
  EXPECT_EQ(-1, compGeneric(VPackValue(i, VPackValueType::SmallInt), i + 1));
  EXPECT_EQ(-1, compGeneric(i, VPackValue(i + 1, VPackValueType::SmallInt)));
  EXPECT_EQ(1, compGeneric(VPackValue(i + 1, VPackValueType::SmallInt), i));
}

TEST(VPackHelperTest, test_small_int_with_utcdate) {
  int64_t i = int64_t{5};
  EXPECT_EQ(1, compGeneric(VPackValue(i, VPackValueType::UTCDate),
                           VPackValue(i, VPackValueType::SmallInt)));
  EXPECT_EQ(-1, compGeneric(VPackValue(i, VPackValueType::SmallInt),
                            VPackValue(i, VPackValueType::UTCDate)));
  EXPECT_EQ(1, compGeneric(VPackValue(i + 1, VPackValueType::UTCDate),
                           VPackValue(i, VPackValueType::SmallInt)));
  EXPECT_EQ(-1, compGeneric(VPackValue(i, VPackValueType::SmallInt),
                            VPackValue(i + 1, VPackValueType::UTCDate)));
  EXPECT_EQ(1, compGeneric(VPackValue(i, VPackValueType::UTCDate),
                           VPackValue(i + 1, VPackValueType::SmallInt)));
  EXPECT_EQ(-1, compGeneric(VPackValue(i + 1, VPackValueType::SmallInt),
                            VPackValue(i, VPackValueType::UTCDate)));
}
