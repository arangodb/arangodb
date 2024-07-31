////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <Aql/AqlValue.h>

#include <velocypack/Builder.h>
#include <cmath>

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

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

using AqlValue = arangodb::aql::AqlValue;

template<typename T>
AqlValue makeAQLValue(T t) {
  return AqlValue(arangodb::aql::AqlValueHintNull());
}

AqlValue makeAQLValue(int64_t x) {
  return AqlValue(arangodb::aql::AqlValueHintInt(x));
}

AqlValue makeAQLValue(uint64_t x) {
  return AqlValue(arangodb::aql::AqlValueHintUInt(x));
}

AqlValue makeAQLValue(double x) {
  VPackBuilder b;
  b.add(VPackValue(x));
  return AqlValue(b.slice());
}

static inline int AqlValueComp(AqlValue& a, AqlValue& b) {
  arangodb::velocypack::Options opt;
  return AqlValue::Compare(&opt, a, b, true);
}

std::string to_string(AqlValue& a) {
  VPackBuilder b;
  arangodb::velocypack::Options opt;
  a.toVelocyPack(&opt, b, true);
  return b.slice().toJson();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test
// suite
// -----------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////////
/// @brief test comparison of numerical values
//////////////////////////////////////////////////////////////////////////////

constexpr uint64_t mantmax = (uint64_t(1) << 52) - 1;

TEST(AqlValueCompareTest, test_comparison_numerical_double) {
  for (int n = 0; n < 2; ++n) {
    // Do everything twice, once with +0 and once with -0

    // We create a vector of numerical velocypack values which is supposed
    // to be sorted strictly ascending. We also check transitivity by
    // comparing all pairs:
    std::vector<AqlValue> v;
    v.push_back(makeAQLValue(makeDoubleValue(1, 2047, 0).d));  // -Inf
    for (uint16_t i = 2046; i >= 1; --i) {
      v.push_back(makeAQLValue(makeDoubleValue(1, i, mantmax).d));
      v.push_back(makeAQLValue(makeDoubleValue(1, i, 0).d));
    }
    v.push_back(
        makeAQLValue(makeDoubleValue(1, 0, mantmax).d));    // - denormalized
    v.push_back(makeAQLValue(makeDoubleValue(1, 0, 1).d));  // - denormalized
    if (n == 0) {
      v.push_back(makeAQLValue(makeDoubleValue(0, 0, 0).d));  // + 0
    } else {
      v.push_back(makeAQLValue(makeDoubleValue(1, 0, 0).d));  // - 0
    }
    v.push_back(makeAQLValue(makeDoubleValue(0, 0, 1).d));  // + denormalized
    v.push_back(
        makeAQLValue(makeDoubleValue(0, 0, mantmax).d));  // + denormalized
    for (uint16_t i = 1; i <= 2046; ++i) {
      v.push_back(makeAQLValue(makeDoubleValue(0, i, 0).d));
      v.push_back(makeAQLValue(makeDoubleValue(0, i, mantmax).d));
    }
    v.push_back(makeAQLValue(makeDoubleValue(0, 2047, 0).d));  // infinity

    // Now check if our comparator agrees that this is strictly ascending:
    for (std::size_t i = 0; i < v.size() - 1; ++i) {
      auto c = AqlValueComp(v[i], v[i + 1]);
      EXPECT_EQ(-1, c) << "Not strictly increasing: " << i << " "
                       << to_string(v[i]) << " " << to_string(v[i + 1]);
      c = AqlValueComp(v[i + 1], v[i]);
      EXPECT_EQ(1, c) << "Not strictly decreasing: " << i << " "
                      << to_string(v[i + 1]) << " " << to_string(v[i]);
    }
    // Check reflexivity:
    for (std::size_t i = 0; i < v.size(); ++i) {
      auto c = AqlValueComp(v[i], v[i]);
      EXPECT_EQ(0, c) << "Not reflexive: " << i << " " << to_string(v[i]);
    }
    // And check transitivity by comparing all pairs:
    for (std::size_t i = 0; i < v.size() - 1; ++i) {
      for (std::size_t j = i + 1; j < v.size(); ++j) {
        auto c = AqlValueComp(v[i], v[j]);
        EXPECT_EQ(-1, c) << "Not transitive: " << i << " " << to_string(v[i])
                         << " " << j << " " << to_string(v[j]);
      }
    }
    // And the same the other way round
    for (std::size_t i = 0; i < v.size() - 1; ++i) {
      for (std::size_t j = i + 1; j < v.size(); ++j) {
        auto c = AqlValueComp(v[j], v[i]);
        EXPECT_EQ(1, c) << "Not transitive: " << i << " " << to_string(v[i])
                        << " " << j << " " << to_string(v[j]);
      }
    }
  }
}

TEST(AqlValueCompareTest, test_equality_zeros) {
  std::vector<AqlValue> v;
  // +0.0:
  v.push_back(makeAQLValue(makeDoubleValue(0, 0, 0).d));
  // -0.0:
  v.push_back(makeAQLValue(makeDoubleValue(1, 0, 0).d));
  // uint64_t{0}:
  v.push_back(makeAQLValue(uint64_t{0}));
  // int64_t{0}:
  v.push_back(makeAQLValue(int64_t{0}));
  for (std::size_t i = 0; i < v.size(); ++i) {
    for (std::size_t j = 0; j < v.size(); ++j) {
      EXPECT_EQ(0, AqlValueComp(v[i], v[j]));
    }
  }
}

TEST(AqlValueCompareTest, test_equality_with_integers) {
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
    AqlValue l = makeAQLValue(i);
    AqlValue r = makeAQLValue(static_cast<double>(i));
    EXPECT_EQ(0, AqlValueComp(l, r));
    EXPECT_EQ(0, AqlValueComp(r, l));
  }
  for (uint64_t u : vu) {
    AqlValue l = makeAQLValue(u);
    AqlValue r = makeAQLValue(static_cast<double>(u));
    EXPECT_EQ(0, AqlValueComp(l, r));
    EXPECT_EQ(0, AqlValueComp(r, l));
  }
}

TEST(AqlValueCompareTest, test_inequality_with_integers) {
  int64_t x = -2;
  uint64_t y = 2;
  for (int i = 0; i < 61; ++i) {
    AqlValue l = makeAQLValue(static_cast<double>(x));
    AqlValue r = makeAQLValue(x - 1);
    EXPECT_EQ(1, AqlValueComp(l, r))
        << "Not less: " << i << " " << to_string(l) << " " << to_string(r);
    EXPECT_EQ(-1, AqlValueComp(r, l))
        << "Not greater: " << i << " " << to_string(r) << " " << to_string(l);
    AqlValue ll = makeAQLValue(y + 1);
    AqlValue rr = makeAQLValue(static_cast<double>(y));
    EXPECT_EQ(1, AqlValueComp(ll, rr))
        << "Not less: " << i << " " << to_string(ll) << " " << to_string(rr);
    EXPECT_EQ(-1, AqlValueComp(rr, ll))
        << "Not greater: " << i << " " << to_string(rr) << " " << to_string(ll);
    x <<= 1;
    y <<= 1;
  }
}

TEST(AqlValueCompareTest, test_numbers_compare_as_doubles) {
  AqlValue a = makeAQLValue(std::numeric_limits<int64_t>::max());

  uint64_t v = std::numeric_limits<int64_t>::max();
  AqlValue b = makeAQLValue(v);

  uint64_t w = v + 1;
  AqlValue c = makeAQLValue(w);

  EXPECT_EQ(0, AqlValueComp(a, b));
  EXPECT_EQ(-1, AqlValueComp(b, c));
  EXPECT_EQ(-1, AqlValueComp(a, c));
}

template<typename T>
static inline void checkNaN(T t) {
  AqlValue nan = makeAQLValue(makeDoubleValue(0, 2047, 1).d);  // NaN
  AqlValue a = makeAQLValue(t);
  EXPECT_EQ(-1, AqlValueComp((a), nan));
  EXPECT_EQ(1, AqlValueComp(nan, (a)));
}

TEST(AqlValueCompareTest, test_nan_greater_than_all) {
  AqlValue a;
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
  AqlValue Ba = makeAQLValue(a);
  AqlValue Bb = makeAQLValue(b);
  arangodb::velocypack::Options opt;
  return AqlValue::Compare(&opt, Ba, Bb, true);
}

TEST(AqlValueCompareTest, test_unsigned_double_comparison) {
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

TEST(AqlValueCompareTest, test_signed_double_comparison) {
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
  AqlValue Ba = makeAQLValue(a);
  AqlValue Bb = makeAQLValue(b);
  arangodb::velocypack::Options opt;
  return AqlValue::Compare(&opt, Ba, Bb, true);
}

TEST(AqlValueCompareTest, test_generic_uses_correct_numerical_comparison) {
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
