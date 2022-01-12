// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/accumulators/count.hpp>
#include <boost/histogram/accumulators/ostream.hpp>
#include <sstream>
#include <thread>
#include "throw_exception.hpp"
#include "utility_str.hpp"

using namespace boost::histogram;
using namespace std::literals;

constexpr int N = 10000;

template <class F>
void parallel(F f) {
  auto g = [&]() {
    for (int i = 0; i < N; ++i) f();
  };

  std::thread a(g), b(g), c(g), d(g);
  a.join();
  b.join();
  c.join();
  d.join();
}

template <class T>
void test_on() {
  using ts_t = accumulators::count<T, true>;

  // default ctor
  {
    ts_t i;
    BOOST_TEST_EQ(i, static_cast<T>(0));
  }

  // ctor from value
  {
    ts_t i{1001};
    BOOST_TEST_EQ(i, static_cast<T>(1001));
    BOOST_TEST_EQ(str(i), "1001"s);
  }

  // add null
  BOOST_TEST_EQ(ts_t{} += ts_t{}, ts_t{});

  // add non-null
  BOOST_TEST_EQ((ts_t{} += ts_t{2}), (ts_t{2}));

  // operator++
  {
    ts_t t;
    parallel([&]() { ++t; });
    BOOST_TEST_EQ(t, static_cast<T>(4 * N));
  }

  // operator+= with value
  {
    ts_t t;
    parallel([&]() { t += 2; });
    BOOST_TEST_EQ(t, static_cast<T>(8 * N));
  }

  // operator+= with another thread-safe
  {
    ts_t t, u;
    u = 2;
    parallel([&]() { t += u; });
    BOOST_TEST_EQ(t, static_cast<T>(8 * N));
  }
}

int main() {
  test_on<int>();
  test_on<float>();

  // copy and assignment from other thread-safe
  {
    accumulators::count<char, true> r{1};
    accumulators::count<int, true> a{r}, b;
    b = r;
    BOOST_TEST_EQ(a, 1);
    BOOST_TEST_EQ(b, 1);
  }

  return boost::report_errors();
}
