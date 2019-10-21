// Copyright 2015-2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/axis.hpp>
#include <boost/histogram/axis/ostream.hpp>
#include <boost/histogram/histogram.hpp>
#include <boost/histogram/make_histogram.hpp>
#include <boost/histogram/ostream.hpp>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>
#include "throw_exception.hpp"
#include "utility_histogram.hpp"

using namespace boost::histogram;

int main() {
  // special stuff that only works with dynamic_tag

  // init with vector of axis and vector of axis::variant
  {
    using R = axis::regular<>;
    using I = axis::integer<>;
    using V = axis::variable<>;

    auto v = std::vector<axis::variant<R, I, V>>();
    v.emplace_back(R{4, -1, 1});
    v.emplace_back(I{1, 7});
    v.emplace_back(V{1, 2, 3});
    auto h = make_histogram(v.begin(), v.end());
    BOOST_TEST_EQ(h.rank(), 3);
    BOOST_TEST_EQ(h.axis(0), v[0]);
    BOOST_TEST_EQ(h.axis(1), v[1]);
    BOOST_TEST_EQ(h.axis(2), v[2]);

    auto h2 = make_histogram_with(std::vector<int>(), v);
    BOOST_TEST_EQ(h2.rank(), 3);
    BOOST_TEST_EQ(h2.axis(0), v[0]);
    BOOST_TEST_EQ(h2.axis(1), v[1]);
    BOOST_TEST_EQ(h2.axis(2), v[2]);

    auto v2 = std::vector<R>();
    v2.emplace_back(10, 0, 1);
    v2.emplace_back(20, 0, 2);
    auto h3 = make_histogram(v2);
    BOOST_TEST_EQ(h3.axis(0), v2[0]);
    BOOST_TEST_EQ(h3.axis(1), v2[1]);
  }

  // bad fill
  {
    auto a = axis::integer<>(0, 1);
    auto b = make(dynamic_tag(), a);
    BOOST_TEST_THROWS(b(0, 0), std::invalid_argument);
    auto c = make(dynamic_tag(), a, a);
    BOOST_TEST_THROWS(c(0), std::invalid_argument);
    auto d = make(dynamic_tag(), a);
    BOOST_TEST_THROWS(d(std::string()), std::invalid_argument);

    struct axis2d {
      auto index(const std::tuple<double, double>& x) const {
        return axis::index_type{std::get<0>(x) == 1 && std::get<1>(x) == 2};
      }
      auto size() const { return axis::index_type{2}; }
    } e;

    auto f = make(dynamic_tag(), a, e);
    BOOST_TEST_THROWS(f(0, 0, 0), std::invalid_argument);
    BOOST_TEST_THROWS(f(0, std::make_tuple(0, 0), 1), std::invalid_argument);
  }

  // bad at
  {
    auto h1 = make(dynamic_tag(), axis::integer<>(0, 2));
    h1(1);
    BOOST_TEST_THROWS(h1.at(0, 0), std::invalid_argument);
    BOOST_TEST_THROWS(h1.at(std::make_tuple(0, 0)), std::invalid_argument);
  }

  // incompatible axis variant methods
  {
    auto c = make(dynamic_tag(), axis::category<std::string>({"A", "B"}));
    BOOST_TEST_THROWS(c.axis().value(0), std::runtime_error);
  }

  {
    auto h = make_histogram(std::vector<axis::integer<>>(1, axis::integer<>(0, 3)));
    h(0);
    h(1);
    h(2);
    BOOST_TEST_EQ(h.at(0), 1);
    BOOST_TEST_EQ(h.at(1), 1);
    BOOST_TEST_EQ(h.at(2), 1);
  }

  return boost::report_errors();
}
