// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/axis/integer.hpp>
#include <boost/histogram/axis/ostream.hpp>
#include <limits>
#include <sstream>
#include <type_traits>
#include "std_ostream.hpp"
#include "throw_exception.hpp"
#include "utility_axis.hpp"
#include "utility_str.hpp"

int main() {
  using namespace boost::histogram;

  BOOST_TEST(std::is_nothrow_move_assignable<axis::integer<>>::value);
  BOOST_TEST(std::is_nothrow_move_constructible<axis::integer<>>::value);

  // bad_ctor
  { BOOST_TEST_THROWS(axis::integer<>(1, -1), std::invalid_argument); }

  // axis::integer with double type
  {
    axis::integer<double> a{-1, 2, "foo"};
    BOOST_TEST_EQ(a.metadata(), "foo");
    BOOST_TEST_EQ(static_cast<const axis::integer<double>&>(a).metadata(), "foo");
    a.metadata() = "bar";
    BOOST_TEST_EQ(static_cast<const axis::integer<double>&>(a).metadata(), "bar");
    BOOST_TEST_EQ(a.bin(-1).lower(), -std::numeric_limits<double>::infinity());
    BOOST_TEST_EQ(a.bin(a.size()).upper(), std::numeric_limits<double>::infinity());
    BOOST_TEST_EQ(a.index(-10), -1);
    BOOST_TEST_EQ(a.index(-2), -1);
    BOOST_TEST_EQ(a.index(-1), 0);
    BOOST_TEST_EQ(a.index(0), 1);
    BOOST_TEST_EQ(a.index(1), 2);
    BOOST_TEST_EQ(a.index(2), 3);
    BOOST_TEST_EQ(a.index(10), 3);
    BOOST_TEST_EQ(a.index(std::numeric_limits<double>::quiet_NaN()), 3);

    BOOST_TEST_EQ(str(a),
                  "integer(-1, 2, metadata=\"bar\", options=underflow | overflow)");

    axis::integer<double> b;
    BOOST_TEST_NE(a, b);
    b = a;
    BOOST_TEST_EQ(a, b);
    axis::integer<double> c = std::move(b);
    BOOST_TEST_EQ(c, a);
    axis::integer<double> d;
    BOOST_TEST_NE(c, d);
    d = std::move(c);
    BOOST_TEST_EQ(d, a);
  }

  // empty axis::integer
  {
    axis::integer<> a;
    BOOST_TEST_EQ(a.size(), 0);
    BOOST_TEST_EQ(a.bin(1), 1);
    BOOST_TEST_EQ(a.bin(0), 0);
    BOOST_TEST_EQ(a.bin(-1), -1);
    BOOST_TEST_EQ(a.index(-10), -1);
    BOOST_TEST_EQ(a.index(-1), -1);
    BOOST_TEST_EQ(a.index(0), 0);
    BOOST_TEST_EQ(a.index(10), 0);

    BOOST_TEST_EQ(str(a), "integer(0, 0, options=underflow | overflow)");

    axis::integer<> b{1, 1};
    BOOST_TEST_EQ(b.size(), 0);
    BOOST_TEST_EQ(b.bin(1), 2);
    BOOST_TEST_EQ(b.bin(0), 1);
    BOOST_TEST_EQ(b.bin(-1), 0);
    BOOST_TEST_EQ(b.index(-10), -1);
    BOOST_TEST_EQ(b.index(-1), -1);
    BOOST_TEST_EQ(b.index(0), -1);
    BOOST_TEST_EQ(b.index(1), 0);
    BOOST_TEST_EQ(b.index(10), 0);

    BOOST_TEST_EQ(str(b), "integer(1, 1, options=underflow | overflow)");
  }

  // axis::integer with int type
  {
    axis::integer<int> a{-1, 2};
    BOOST_TEST_EQ(a.bin(-2), -3);
    BOOST_TEST_EQ(a.bin(4), 3);
    BOOST_TEST_EQ(a.index(-10), -1);
    BOOST_TEST_EQ(a.index(-2), -1);
    BOOST_TEST_EQ(a.index(-1), 0);
    BOOST_TEST_EQ(a.index(0), 1);
    BOOST_TEST_EQ(a.index(1), 2);
    BOOST_TEST_EQ(a.index(2), 3);
    BOOST_TEST_EQ(a.index(10), 3);

    BOOST_TEST_EQ(str(a), "integer(-1, 2, options=underflow | overflow)");
  }

  // axis::integer int,circular
  {
    axis::integer<int, axis::null_type, axis::option::circular_t> a(-1, 1);
    BOOST_TEST_EQ(a.value(-1), -2);
    BOOST_TEST_EQ(a.value(0), -1);
    BOOST_TEST_EQ(a.value(1), 0);
    BOOST_TEST_EQ(a.value(2), 1);
    BOOST_TEST_EQ(a.value(3), 2);
    BOOST_TEST_EQ(a.index(-2), 1);
    BOOST_TEST_EQ(a.index(-1), 0);
    BOOST_TEST_EQ(a.index(0), 1);
    BOOST_TEST_EQ(a.index(1), 0);
    BOOST_TEST_EQ(a.index(2), 1);

    BOOST_TEST_EQ(str(a), "integer(-1, 1, options=circular)");
  }

  // axis::integer double,circular
  {
    axis::integer<double, axis::null_type, axis::option::circular_t> a(-1, 1);
    BOOST_TEST_EQ(a.value(-1), -2);
    BOOST_TEST_EQ(a.value(0), -1);
    BOOST_TEST_EQ(a.value(1), 0);
    BOOST_TEST_EQ(a.value(2), 1);
    BOOST_TEST_EQ(a.value(3), 2);
    BOOST_TEST_EQ(a.index(-2), 1);
    BOOST_TEST_EQ(a.index(-1.9), 1);
    BOOST_TEST_EQ(a.index(-1.1), 1);
    BOOST_TEST_EQ(a.index(-1), 0);
    BOOST_TEST_EQ(a.index(0), 1);
    BOOST_TEST_EQ(a.index(1), 0);
    BOOST_TEST_EQ(a.index(2), 1);
    BOOST_TEST_EQ(a.index(2.1), 1);
    BOOST_TEST_EQ(a.index(2.9), 1);

    BOOST_TEST_EQ(a.index(std::numeric_limits<double>::quiet_NaN()), 2);
  }

  // axis::integer with growth
  {
    using pii_t = std::pair<axis::index_type, axis::index_type>;
    axis::integer<double, axis::null_type, axis::option::growth_t> a;
    BOOST_TEST_EQ(a.size(), 0);
    BOOST_TEST_EQ(a.update(0), pii_t(0, -1));
    BOOST_TEST_EQ(a.size(), 1);
    BOOST_TEST_EQ(a.update(1), pii_t(1, -1));
    BOOST_TEST_EQ(a.size(), 2);
    BOOST_TEST_EQ(a.update(-1), pii_t(0, 1));
    BOOST_TEST_EQ(a.size(), 3);
    BOOST_TEST_EQ(a.update(std::numeric_limits<double>::infinity()), pii_t(a.size(), 0));
    BOOST_TEST_EQ(a.update(std::numeric_limits<double>::quiet_NaN()), pii_t(a.size(), 0));
    BOOST_TEST_EQ(a.update(-std::numeric_limits<double>::infinity()), pii_t(-1, 0));
  }

  // iterators
  {
    test_axis_iterator(axis::integer<int>(0, 4), 0, 4);
    test_axis_iterator(axis::integer<double>(0, 4), 0, 4);
    test_axis_iterator(
        axis::integer<int, axis::null_type, axis::option::circular_t>(0, 4), 0, 4);
  }

  // shrink and rebin
  {
    using A = axis::integer<>;
    auto a = A(-1, 5);
    auto b = A(a, 2, 5, 1);
    BOOST_TEST_EQ(b.size(), 3);
    BOOST_TEST_EQ(b.value(0), 1);
    BOOST_TEST_EQ(b.value(3), 4);
    auto c = A(a, 1, 5, 1);
    BOOST_TEST_EQ(c.size(), 4);
    BOOST_TEST_EQ(c.value(0), 0);
    BOOST_TEST_EQ(c.value(4), 4);
    auto e = A(a, 2, 5, 1);
    BOOST_TEST_EQ(e.size(), 3);
    BOOST_TEST_EQ(e.value(0), 1);
    BOOST_TEST_EQ(e.value(3), 4);
  }

  // shrink and rebin with circular option
  {
    using A = axis::integer<int, axis::null_type, axis::option::circular_t>;
    auto a = A(1, 5);
    auto b = A(a, 0, 4, 1);
    BOOST_TEST_EQ(a, b);
    BOOST_TEST_THROWS(A(a, 1, 4, 1), std::invalid_argument);
    BOOST_TEST_THROWS(A(a, 0, 3, 1), std::invalid_argument);
    BOOST_TEST_THROWS(A(a, 0, 4, 2), std::invalid_argument);
  }

  return boost::report_errors();
}
