// Copyright 2020 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/axis/boolean.hpp>
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

  BOOST_TEST(std::is_nothrow_move_assignable<axis::boolean<>>::value);
  BOOST_TEST(std::is_nothrow_move_constructible<axis::boolean<>>::value);

  // axis::integer with double type
  {
    axis::boolean<> a{"foo"};
    BOOST_TEST_EQ(a.metadata(), "foo");
    a.metadata() = "bar";
    const auto& cref = a;
    BOOST_TEST_EQ(cref.metadata(), "bar");
    cref.metadata() = "foo";
    BOOST_TEST_EQ(cref.metadata(), "foo");

    BOOST_TEST_EQ(a.index(true), 1);
    BOOST_TEST_EQ(a.index(false), 0);
    BOOST_TEST_EQ(a.index(1), 1);
    BOOST_TEST_EQ(a.index(0), 0);

    BOOST_TEST_EQ(a.options(), axis::option::none_t::value);

    BOOST_TEST_CSTR_EQ(str(a).c_str(), "boolean(metadata=\"foo\")");

    axis::boolean<> b;
    BOOST_TEST_CSTR_EQ(str(b).c_str(), "boolean()");

    BOOST_TEST_NE(a, b);
    b = a;
    BOOST_TEST_EQ(a, b);
    axis::boolean<> c = std::move(b);
    BOOST_TEST_EQ(c, a);
    axis::boolean<> d;
    BOOST_TEST_NE(c, d);
    d = std::move(c);
    BOOST_TEST_EQ(d, a);
  }

  // iterators
  test_axis_iterator(axis::boolean<>(), 0, 2);

  return boost::report_errors();
}
