// Copyright 2015-2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/axis/ostream.hpp>
#include <boost/histogram/axis/variable.hpp>
#include <limits>
#include <sstream>
#include <type_traits>
#include <vector>
#include "is_close.hpp"
#include "std_ostream.hpp"
#include "throw_exception.hpp"
#include "utility_axis.hpp"
#include "utility_str.hpp"

using namespace boost::histogram;

int main() {
  constexpr auto inf = std::numeric_limits<double>::infinity();
  constexpr auto nan = std::numeric_limits<double>::quiet_NaN();

  BOOST_TEST(std::is_nothrow_move_assignable<axis::variable<>>::value);
  BOOST_TEST(std::is_nothrow_move_constructible<axis::variable<>>::value);

  // bad_ctors
  {
    BOOST_TEST_THROWS(axis::variable<>(std::vector<double>{}), std::invalid_argument);
    BOOST_TEST_THROWS(axis::variable<>({1.0}), std::invalid_argument);
    BOOST_TEST_THROWS(axis::variable<>({1.0, 1.0}), std::invalid_argument);
    BOOST_TEST_THROWS(axis::variable<>({1.0, -1.0}), std::invalid_argument);
    BOOST_TEST_THROWS((axis::variable<>{{1.0, 2.0, nan}}), std::invalid_argument);
    BOOST_TEST_THROWS((axis::variable<>{{1.0, nan, 2.0}}), std::invalid_argument);
    BOOST_TEST_THROWS((axis::variable<>{{nan, 1.0, 2.0}}), std::invalid_argument);
    BOOST_TEST_THROWS((axis::variable<>{{inf, inf}}), std::invalid_argument);
  }

  // axis::variable
  {
    axis::variable<> a{{-1, 0, 1}, "foo"};
    BOOST_TEST_EQ(a.size(), 2);
    BOOST_TEST_EQ(a.metadata(), "foo");
    BOOST_TEST_EQ(static_cast<const axis::variable<>&>(a).metadata(), "foo");
    a.metadata() = "bar";
    BOOST_TEST_EQ(static_cast<const axis::variable<>&>(a).metadata(), "bar");
    BOOST_TEST_EQ(a.bin(-1).lower(), -inf);
    BOOST_TEST_EQ(a.bin(a.size()).upper(), inf);
    BOOST_TEST_EQ(a.value(0), -1);
    BOOST_TEST_EQ(a.value(0.5), -0.5);
    BOOST_TEST_EQ(a.value(1), 0);
    BOOST_TEST_EQ(a.value(1.5), 0.5);
    BOOST_TEST_EQ(a.value(2), 1);
    BOOST_TEST_EQ(a.index(-10), -1);
    BOOST_TEST_EQ(a.index(-1), 0);
    BOOST_TEST_EQ(a.index(0), 1);
    BOOST_TEST_EQ(a.index(1), 2);
    BOOST_TEST_EQ(a.index(10), 2);
    BOOST_TEST_EQ(a.index(-inf), -1);
    BOOST_TEST_EQ(a.index(inf), 2);
    BOOST_TEST_EQ(a.index(nan), 2);

    BOOST_TEST_EQ(str(a),
                  "variable(-1, 0, 1, metadata=\"bar\", options=underflow | overflow)");

    axis::variable<> b;
    BOOST_TEST_NE(a, b);
    b = a;
    BOOST_TEST_EQ(a, b);
    axis::variable<> c = std::move(b);
    BOOST_TEST_EQ(c, a);
    axis::variable<> d;
    BOOST_TEST_NE(c, d);
    d = std::move(c);
    BOOST_TEST_EQ(d, a);
    axis::variable<> e{-2, 0, 2};
    BOOST_TEST_NE(a, e);
  }

  // infinity values in constructor
  {
    axis::variable<> a{{-inf, 1.0, 2.0, inf}};
    BOOST_TEST_EQ(a.index(-inf), 0);
    BOOST_TEST_EQ(a.index(0.0), 0);
    BOOST_TEST_EQ(a.index(1.0), 1);
    BOOST_TEST_EQ(a.index(2.0), 2);
    BOOST_TEST_EQ(a.index(inf), 3);

    BOOST_TEST_EQ(a.value(-1), -inf);
    BOOST_TEST_EQ(a.value(-0.5), -inf);
    BOOST_TEST_EQ(a.value(0), -inf);
    BOOST_TEST_EQ(a.value(0.5), -inf);
    BOOST_TEST_EQ(a.value(1), 1.0);
    BOOST_TEST_EQ(a.value(1.5), 1.5);
    BOOST_TEST_EQ(a.value(2), 2.0);
    BOOST_TEST_EQ(a.value(2.5), inf);
    BOOST_TEST_EQ(a.value(3), inf);
    BOOST_TEST_EQ(a.value(4), inf);

    BOOST_TEST_EQ(str(a), "variable(-inf, 1, 2, inf, options=underflow | overflow)");
  }

  // axis::variable circular
  {
    axis::variable<double, axis::null_type, axis::option::circular_t> a{-1, 1, 2};
    BOOST_TEST_EQ(a.value(-2), -4);
    BOOST_TEST_EQ(a.value(-1), -2);
    BOOST_TEST_EQ(a.value(0), -1);
    BOOST_TEST_EQ(a.value(1), 1);
    BOOST_TEST_EQ(a.value(2), 2);
    BOOST_TEST_EQ(a.value(3), 4);
    BOOST_TEST_EQ(a.value(4), 5);
    BOOST_TEST_EQ(a.index(-3), 0); // -3 + 3 = 0
    BOOST_TEST_EQ(a.index(-2), 1); // -2 + 3 = 1
    BOOST_TEST_EQ(a.index(-1), 0);
    BOOST_TEST_EQ(a.index(0), 0);
    BOOST_TEST_EQ(a.index(1), 1);
    BOOST_TEST_EQ(a.index(2), 0);
    BOOST_TEST_EQ(a.index(3), 0); // 3 - 3 = 0
    BOOST_TEST_EQ(a.index(4), 1); // 4 - 3 = 1
  }

  // axis::regular with growth
  {
    using pii_t = std::pair<axis::index_type, axis::index_type>;
    axis::variable<double, axis::null_type, axis::option::growth_t> a{0, 1};
    BOOST_TEST_EQ(a.size(), 1);
    BOOST_TEST_EQ(a.update(0), pii_t(0, 0));
    BOOST_TEST_EQ(a.size(), 1);
    BOOST_TEST_EQ(a.update(1.1), pii_t(1, -1));
    BOOST_TEST_EQ(a.size(), 2);
    BOOST_TEST_EQ(a.value(0), 0);
    BOOST_TEST_EQ(a.value(1), 1);
    BOOST_TEST_EQ(a.value(2), 1.5);
    BOOST_TEST_EQ(a.update(-0.1), pii_t(0, 1));
    BOOST_TEST_EQ(a.value(0), -0.5);
    BOOST_TEST_EQ(a.size(), 3);
    BOOST_TEST_EQ(a.update(10), pii_t(3, -1));
    BOOST_TEST_EQ(a.size(), 4);
    BOOST_TEST_IS_CLOSE(a.value(4), 10, 1e-9);
    BOOST_TEST_EQ(a.update(-10), pii_t(0, 1));
    BOOST_TEST_EQ(a.size(), 5);
    BOOST_TEST_IS_CLOSE(a.value(0), -10, 1e-9);

    BOOST_TEST_EQ(a.update(-inf), pii_t(-1, 0));
    BOOST_TEST_EQ(a.update(inf), pii_t(a.size(), 0));
    BOOST_TEST_EQ(a.update(nan), pii_t(a.size(), 0));
  }

  // iterators
  {
    test_axis_iterator(axis::variable<>{1, 2, 3}, 0, 2);
    test_axis_iterator(
        axis::variable<double, axis::null_type, axis::option::circular_t>{1, 2, 3}, 0, 2);
  }

  // shrink and rebin
  {
    using A = axis::variable<>;
    auto a = A({0, 1, 2, 3, 4, 5});
    auto b = A(a, 1, 4, 1);
    BOOST_TEST_EQ(b.size(), 3);
    BOOST_TEST_EQ(b.value(0), 1);
    BOOST_TEST_EQ(b.value(3), 4);
    auto c = A(a, 0, 4, 2);
    BOOST_TEST_EQ(c.size(), 2);
    BOOST_TEST_EQ(c.value(0), 0);
    BOOST_TEST_EQ(c.value(2), 4);
    auto e = A(a, 1, 5, 2);
    BOOST_TEST_EQ(e.size(), 2);
    BOOST_TEST_EQ(e.value(0), 1);
    BOOST_TEST_EQ(e.value(2), 5);
  }

  // shrink and rebin with circular option
  {
    using A = axis::variable<double, axis::null_type, axis::option::circular_t>;
    auto a = A({1, 2, 3, 4, 5});
    BOOST_TEST_THROWS(A(a, 1, 4, 1), std::invalid_argument);
    BOOST_TEST_THROWS(A(a, 0, 3, 1), std::invalid_argument);
    auto b = A(a, 0, 4, 2);
    BOOST_TEST_EQ(b.size(), 2);
    BOOST_TEST_EQ(b.value(0), 1);
    BOOST_TEST_EQ(b.value(2), 5);
  }

  return boost::report_errors();
}
