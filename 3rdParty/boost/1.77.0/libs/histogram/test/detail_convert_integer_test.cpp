// Copyright 2015-2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/histogram/detail/convert_integer.hpp>
#include "std_ostream.hpp"

using namespace boost::histogram::detail;

int main() {

  BOOST_TEST_TRAIT_SAME(convert_integer<char, float>, float);
  BOOST_TEST_TRAIT_SAME(convert_integer<int, float>, float);
  BOOST_TEST_TRAIT_SAME(convert_integer<double, float>, double);
  BOOST_TEST_TRAIT_SAME(convert_integer<float, double>, float);

  return boost::report_errors();
}
