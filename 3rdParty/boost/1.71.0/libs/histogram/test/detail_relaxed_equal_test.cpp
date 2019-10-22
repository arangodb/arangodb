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
  struct A {};
  A a, b;

  struct B {
    bool operator==(const B&) const { return false; }
  };
  B c, d;

  BOOST_TEST(relaxed_equal(a, b));
  BOOST_TEST_NOT(relaxed_equal(c, d));

  return boost::report_errors();
}
