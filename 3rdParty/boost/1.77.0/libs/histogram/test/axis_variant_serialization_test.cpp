// Copyright (c) 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// This test is inspired by the corresponding boost/beast test of detail_variant.

#include <cassert>
#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/axis/integer.hpp>
#include <boost/histogram/axis/ostream.hpp>
#include <boost/histogram/axis/regular.hpp>
#include <boost/histogram/axis/variant.hpp>
#include <boost/histogram/serialization.hpp>
#include "throw_exception.hpp"
#include "utility_axis.hpp"
#include "utility_serialization.hpp"

using namespace boost::histogram::axis;

int main(int argc, char** argv) {
  assert(argc == 2);

  const auto filename = join(argv[1], "axis_variant_serialization_test.xml");

  using R = regular<>;
  using I = integer<>;

  variant<I, R> a(I(0, 3));
  variant<I, R> b(R(1, 0, 1));
  print_xml(filename, b);
  BOOST_TEST_NE(a, b);
  load_xml(filename, a);
  BOOST_TEST_EQ(a, b);

  variant<I> c; // load incompatible version
  BOOST_TEST_THROWS(load_xml(filename, c), std::runtime_error);

  return boost::report_errors();
}
