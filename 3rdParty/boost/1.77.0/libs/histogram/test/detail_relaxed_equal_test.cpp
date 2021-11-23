// Copyright 2015-2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/histogram/detail/relaxed_equal.hpp>
#include "std_ostream.hpp"

using namespace boost::histogram::detail;

int main() {
  struct Stateless {
  } a, b;

  struct Stateful {
    int state; // has state
  } c, d;

  struct HasEqual {
    int state;
    bool operator==(const HasEqual& rhs) const { return state == rhs.state; }
  } e{1}, f{1}, g{2};

  BOOST_TEST(relaxed_equal{}(a, b));
  BOOST_TEST_NOT(relaxed_equal{}(a, c));
  BOOST_TEST_NOT(relaxed_equal{}(c, d));
  BOOST_TEST(relaxed_equal{}(e, f));
  BOOST_TEST_NOT(relaxed_equal{}(e, g));

  return boost::report_errors();
}
