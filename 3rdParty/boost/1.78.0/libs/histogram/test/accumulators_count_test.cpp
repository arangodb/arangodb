// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/accumulators/count.hpp>
#include <boost/histogram/accumulators/ostream.hpp>
#include "throw_exception.hpp"
#include "utility_str.hpp"

using namespace boost::histogram;
using namespace std::literals;

template <class T, bool B>
void run_tests() {
  using c_t = accumulators::count<T, B>;

  {
    c_t c;
    ++c;
    BOOST_TEST_EQ(c.value(), 1);
    BOOST_TEST_EQ(str(c), "1"s);
    BOOST_TEST_EQ(str(c, 2, false), " 1"s);
    BOOST_TEST_EQ(str(c, 2, true), "1 "s);

    c += 2;
    BOOST_TEST_EQ(str(c), "3"s);

    BOOST_TEST_EQ(c, static_cast<T>(3));
    BOOST_TEST_NE(c, static_cast<T>(2));
  }

  {
    c_t one(1), two(2), one_copy(1);
    BOOST_TEST_LT(one, two);
    BOOST_TEST_LE(one, two);
    BOOST_TEST_LE(one, one_copy);
    BOOST_TEST_GT(two, one);
    BOOST_TEST_GE(two, one);
    BOOST_TEST_GE(one, one_copy);
  }

  BOOST_TEST_EQ(c_t{} += c_t{}, c_t{});

  {
    c_t two(2);
    auto six = two * 3;
    BOOST_TEST_EQ(six, static_cast<T>(6));
    six *= 2;
    BOOST_TEST_EQ(six, static_cast<T>(12));
  }

  {
    c_t six(6);
    auto two = six / 3;
    BOOST_TEST_EQ(two, static_cast<T>(2));
    two /= 2;
    BOOST_TEST_EQ(two, static_cast<T>(1));
  }
}

int main() {
  run_tests<int, false>();
  run_tests<int, true>();
  run_tests<float, false>();
  run_tests<float, true>();

  return boost::report_errors();
}
