// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/histogram/detail/operators.hpp>
#include <limits>
#include <ostream>

using namespace boost::histogram::detail;

struct TotallyOrdered : totally_ordered<TotallyOrdered, TotallyOrdered, void> {
  TotallyOrdered(int i) : x(i) {}

  bool operator<(const TotallyOrdered& o) const noexcept { return x < o.x; }
  bool operator==(const TotallyOrdered& o) const noexcept { return x == o.x; }
  bool operator<(const int& o) const noexcept { return x < o; }
  bool operator>(const int& o) const noexcept { return x > o; }
  bool operator==(const int& o) const noexcept { return x == o; }

  int x;
};

std::ostream& operator<<(std::ostream& os, const TotallyOrdered& t) {
  os << t.x;
  return os;
}

struct PartiallyOrdered : partially_ordered<PartiallyOrdered, PartiallyOrdered, void> {
  PartiallyOrdered(double i) : x(i) {}

  bool operator<(const PartiallyOrdered& o) const noexcept { return x < o.x; }
  bool operator==(const PartiallyOrdered& o) const noexcept { return x == o.x; }
  bool operator<(const double& o) const noexcept { return x < o; }
  bool operator>(const double& o) const noexcept { return x > o; }
  bool operator==(const double& o) const noexcept { return x == o; }

  double x;
};

std::ostream& operator<<(std::ostream& os, const PartiallyOrdered& t) {
  os << t.x;
  return os;
}

template <class T, class U>
void test_nan(std::false_type) {}

template <class T, class U>
void test_nan(std::true_type) {
  const U nan = std::numeric_limits<U>::quiet_NaN();

  // IEEE 754: Any comparison with nan is false, except != which is true.
  // But some of the following tests fail on MSVC:
  //   BOOST_TEST_NOT(nan < nan); // true, should be false
  //   BOOST_TEST_NOT(nan > nan);
  //   BOOST_TEST_NOT(nan == nan);
  //   BOOST_TEST_NOT(nan <= nan); // true, should be false
  //   BOOST_TEST_NOT(nan >= nan);
  //   BOOST_TEST(nan != nan);
  // Probably related:
  // https://developercommunity.visualstudio.com/content/problem/445462/nan-nan-is-constant-folded-to-true-but-should-prob.html
  // The tests below don't fail probably because constant-folding doesn't happen.

  BOOST_TEST_NOT(T(1.0) < T(nan));
  BOOST_TEST_NOT(T(1.0) > T(nan));
  BOOST_TEST_NOT(T(1.0) == T(nan));
  BOOST_TEST_NOT(T(1.0) <= T(nan));
  BOOST_TEST_NOT(T(1.0) >= T(nan));
  BOOST_TEST(T(1.0) != T(nan));

  BOOST_TEST_NOT(T(nan) < 1.0);
  BOOST_TEST_NOT(T(nan) > 1.0);
  BOOST_TEST_NOT(T(nan) == 1.0);
  BOOST_TEST_NOT(T(nan) <= 1.0);
  BOOST_TEST_NOT(T(nan) >= 1.0);
  BOOST_TEST(T(nan) != 1.0);

  BOOST_TEST_NOT(1.0 < T(nan));
  BOOST_TEST_NOT(1.0 > T(nan));
  BOOST_TEST_NOT(1.0 == T(nan));
  BOOST_TEST_NOT(1.0 <= T(nan));
  BOOST_TEST_NOT(1.0 >= T(nan));
  BOOST_TEST(1.0 != T(nan));

  BOOST_TEST_NOT(T(nan) < T(nan));
  BOOST_TEST_NOT(T(nan) > T(nan));
  BOOST_TEST_NOT(T(nan) == T(nan));
  BOOST_TEST_NOT(T(nan) <= T(nan));
  BOOST_TEST_NOT(T(nan) >= T(nan));
  BOOST_TEST(T(nan) != T(nan));

  BOOST_TEST_NOT(T(nan) < nan);
  BOOST_TEST_NOT(T(nan) > nan);
  BOOST_TEST_NOT(T(nan) == nan);
  BOOST_TEST_NOT(T(nan) <= nan);
  BOOST_TEST_NOT(T(nan) >= nan);
  BOOST_TEST(T(nan) != nan);

  BOOST_TEST_NOT(nan < T(nan));
  BOOST_TEST_NOT(nan > T(nan));
  BOOST_TEST_NOT(nan == T(nan));
  BOOST_TEST_NOT(nan <= T(nan));
  BOOST_TEST_NOT(nan >= T(nan));
  BOOST_TEST(nan != T(nan));
}

template <class T, class U>
void test() {
  T x{1};
  U e{1}, l{0}, u{2};
  BOOST_TEST_EQ(x, e);
  BOOST_TEST_NE(x, l);
  BOOST_TEST_LT(x, u);
  BOOST_TEST_GT(x, l);
  BOOST_TEST_LE(x, e);
  BOOST_TEST_LE(x, u);
  BOOST_TEST_GE(x, e);
  BOOST_TEST_GE(x, l);

  BOOST_TEST_EQ(e, x);
  BOOST_TEST_NE(l, x);
  BOOST_TEST_LT(l, x);
  BOOST_TEST_GT(u, x);
  BOOST_TEST_LE(e, x);
  BOOST_TEST_LE(l, x);
  BOOST_TEST_GE(e, x);
  BOOST_TEST_GE(u, x);

  test_nan<T, U>(std::is_floating_point<U>{});
}

int main() {
  test<TotallyOrdered, TotallyOrdered>();
  test<TotallyOrdered, int>();
  test<PartiallyOrdered, PartiallyOrdered>();
  test<PartiallyOrdered, double>();

  return boost::report_errors();
}
