// Copyright 2015-2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <array>
#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/accumulators/weighted_mean.hpp>
#include <boost/histogram/accumulators/weighted_sum.hpp>
#include "throw_exception.hpp"
#include <boost/histogram/storage_adaptor.hpp>
#include <boost/histogram/unlimited_storage.hpp>
#include <cmath>
#include <deque>
#include <limits>
#include <map>
#include <sstream>
#include <vector>
#include "is_close.hpp"
#include "utility_allocator.hpp"

using namespace boost::histogram;
using namespace std::literals;

template <class T>
auto str(const T& t) {
  std::ostringstream os;
  os << t;
  return os.str();
}

template <typename T>
void tests() {
  using Storage = storage_adaptor<T>;
  // ctor, copy, move
  {
    Storage a;
    a.reset(2);
    Storage b(a);
    Storage c;
    c = a;
    BOOST_TEST_EQ(std::distance(a.begin(), a.end()), 2);
    BOOST_TEST_EQ(a.size(), 2);
    BOOST_TEST_EQ(b.size(), 2);
    BOOST_TEST_EQ(c.size(), 2);

    Storage d(std::move(a));
    BOOST_TEST_EQ(d.size(), 2);
    Storage e;
    e = std::move(d);
    BOOST_TEST_EQ(e.size(), 2);

    const auto t = T();
    storage_adaptor<T> g(t); // tests converting ctor
    BOOST_TEST_EQ(g.size(), 0);
    const auto u = std::vector<typename Storage::value_type>(3, 1);
    Storage h(u); // tests converting ctor
    BOOST_TEST_EQ(h.size(), 3);
    BOOST_TEST_EQ(h[0], 1);
    BOOST_TEST_EQ(h[1], 1);
    BOOST_TEST_EQ(h[2], 1);
  }

  // increment, add, sub, set, reset, compare
  {
    Storage a;
    a.reset(1);
    ++a[0];
    const auto save = a[0]++;
    BOOST_TEST_EQ(save, 1);
    BOOST_TEST_EQ(a[0], 2);
    a.reset(2);
    BOOST_TEST_EQ(a.size(), 2);
    ++a[0];
    a[0] += 2;
    a[1] += 5;
    BOOST_TEST_EQ(a[0], 3);
    BOOST_TEST_EQ(a[1], 5);
    a[0] -= 2;
    a[1] -= 5;
    BOOST_TEST_EQ(a[0], 1);
    BOOST_TEST_EQ(a[1], 0);
    a[1] = 9;
    BOOST_TEST_EQ(a[0], 1);
    BOOST_TEST_EQ(a[1], 9);
    BOOST_TEST_LT(a[0], 2);
    BOOST_TEST_LT(0, a[1]);
    BOOST_TEST_GT(a[1], 4);
    BOOST_TEST_GT(3, a[0]);
    a[1] = a[0];
    BOOST_TEST_EQ(a[0], 1);
    BOOST_TEST_EQ(a[1], 1);
    a.reset(0);
    BOOST_TEST_EQ(a.size(), 0);
  }

  // copy
  {
    Storage a;
    a.reset(1);
    ++a[0];
    Storage b;
    b.reset(2);
    BOOST_TEST(!(a == b));
    b = a;
    BOOST_TEST(a == b);
    BOOST_TEST_EQ(b.size(), 1);
    BOOST_TEST_EQ(b[0], 1);

    Storage c(a);
    BOOST_TEST(a == c);
    BOOST_TEST_EQ(c.size(), 1);
    BOOST_TEST_EQ(c[0], 1);
  }

  // move
  {
    Storage a;
    a.reset(1);
    ++a[0];
    Storage b;
    BOOST_TEST(!(a == b));
    b = std::move(a);
    BOOST_TEST_EQ(b.size(), 1);
    BOOST_TEST_EQ(b[0], 1);
    Storage c(std::move(b));
    BOOST_TEST_EQ(c.size(), 1);
    BOOST_TEST_EQ(c[0], 1);
  }

  {
    Storage a;
    a.reset(1);
    a[0] += 2;
    BOOST_TEST_EQ(str(a[0]), "2"s);
  }
}

template <typename A, typename B>
void mixed_tests() {
  // comparison
  {
    A a, b;
    a.reset(1);
    b.reset(1);
    B c, d;
    c.reset(1);
    d.reset(2);
    ++a[0];
    ++b[0];
    c[0] += 2;
    d[0] = 3;
    d[1] = 5;
    BOOST_TEST_EQ(a[0], 1);
    BOOST_TEST_EQ(b[0], 1);
    BOOST_TEST_EQ(c[0], 2);
    BOOST_TEST_EQ(d[0], 3);
    BOOST_TEST_EQ(d[1], 5);
    BOOST_TEST(a == a);
    BOOST_TEST(a == b);
    BOOST_TEST(!(a == c));
    BOOST_TEST(!(a == d));
  }

  // ctor, copy, move, assign
  {
    A a;
    a.reset(2);
    ++a[1];
    B b(a);
    B c;
    c = a;
    BOOST_TEST_EQ(c[0], 0);
    BOOST_TEST_EQ(c[1], 1);
    c = A();
    BOOST_TEST_EQ(c.size(), 0);
    B d(std::move(a));
    B e;
    e = std::move(d);
    BOOST_TEST_EQ(e[0], 0);
    BOOST_TEST_EQ(e[1], 1);
  }
}

int main() {
  tests<std::vector<int>>();
  tests<std::array<int, 100>>();
  tests<std::deque<int>>();
  tests<std::map<std::size_t, int>>();
  tests<std::unordered_map<std::size_t, int>>();

  mixed_tests<storage_adaptor<std::vector<int>>,
              storage_adaptor<std::array<double, 100>>>();
  mixed_tests<unlimited_storage<>, storage_adaptor<std::vector<double>>>();
  mixed_tests<storage_adaptor<std::vector<int>>, unlimited_storage<>>();
  mixed_tests<storage_adaptor<std::vector<int>>,
              storage_adaptor<std::map<std::size_t, int>>>();

  // special case for division of map-based storage_adaptor
  {
    auto a = storage_adaptor<std::map<std::size_t, double>>();
    a.reset(2);
    a[0] /= 2;
    BOOST_TEST_EQ(a[0], 0);
    a[0] = 2;
    a[0] /= 2;
    BOOST_TEST_EQ(a[0], 1);
    a[1] /= std::numeric_limits<double>::quiet_NaN();
    BOOST_TEST(std::isnan(static_cast<double>(a[1])));
  }

  // with accumulators::weighted_sum
  {
    auto a = storage_adaptor<std::vector<accumulators::weighted_sum<double>>>();
    a.reset(1);
    ++a[0];
    a[0] += 1;
    a[0] += 2;
    a[0] += accumulators::weighted_sum<double>(1, 0);
    BOOST_TEST_EQ(a[0].value(), 5);
    BOOST_TEST_EQ(a[0].variance(), 6);
    a[0] *= 2;
    BOOST_TEST_EQ(a[0].value(), 10);
    BOOST_TEST_EQ(a[0].variance(), 24);
  }

  // with accumulators::weighted_mean
  {
    auto a = storage_adaptor<std::vector<accumulators::weighted_mean<double>>>();
    a.reset(1);
    a[0](/* sample */ 1);
    a[0](/* weight */ 2, /* sample */ 2);
    a[0] += accumulators::weighted_mean<>(1, 0, 0, 0);
    BOOST_TEST_EQ(a[0].sum_of_weights(), 4);
    BOOST_TEST_IS_CLOSE(a[0].value(), 1.25, 1e-3);
    BOOST_TEST_IS_CLOSE(a[0].variance(), 0.242, 1e-3);
  }

  // exceeding array capacity
  {
    auto a = storage_adaptor<std::array<int, 10>>();
    a.reset(10); // should not throw
    BOOST_TEST_THROWS(a.reset(11), std::length_error);
    auto b = storage_adaptor<std::vector<int>>();
    b.reset(11);
    BOOST_TEST_THROWS(a = b, std::length_error);
  }

  // test sparsity of map backend
  {
    tracing_allocator_db db;
    tracing_allocator<char> alloc(db);
    using map_t = std::map<std::size_t, double, std::less<std::size_t>,
                           tracing_allocator<std::pair<const std::size_t, double>>>;
    using A = storage_adaptor<map_t>;
    auto a = A(alloc);
    // MSVC implementation allocates some structures for debugging
    const auto baseline = db.second;
    a.reset(10);
    BOOST_TEST_EQ(db.first, baseline); // nothing allocated yet
    // queries do not allocate
    BOOST_TEST_EQ(a[0], 0);
    BOOST_TEST_EQ(a[9], 0);
    BOOST_TEST_EQ(db.first, baseline);
    ++a[5]; // causes one allocation
    const auto node = db.first - baseline;
    BOOST_TEST_EQ(a[5], 1);
    a[4] += 2; // causes one allocation
    BOOST_TEST_EQ(a[4], 2);
    BOOST_TEST_EQ(db.first, baseline + 2 * node);
    a[3] -= 2; // causes one allocation
    BOOST_TEST_EQ(a[3], -2);
    BOOST_TEST_EQ(db.first, baseline + 3 * node);
    a[2] *= 2; // no allocation
    BOOST_TEST_EQ(db.first, baseline + 3 * node);
    a[2] /= 2; // no allocation
    BOOST_TEST_EQ(db.first, baseline + 3 * node);
    a[4] = 0; // causes one deallocation
    BOOST_TEST_EQ(db.first, baseline + 2 * node);

    auto b = storage_adaptor<std::vector<int>>();
    b.reset(5);
    ++b[2];
    a = b;
    // only one new allocation for non-zero value
    BOOST_TEST_EQ(db.first, baseline + node);
  }

  return boost::report_errors();
}
