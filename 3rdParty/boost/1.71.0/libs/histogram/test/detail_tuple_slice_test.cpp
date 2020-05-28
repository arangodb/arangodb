// Copyright 2015-2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/detail/tuple_slice.hpp>
#include <tuple>
#include "std_ostream.hpp"

using namespace boost::histogram::detail;

int main() {

  auto a = std::make_tuple(1, 2, 3, 4);
  const auto b = tuple_slice<1, 2>(a);
  BOOST_TEST_EQ(b, std::make_tuple(2, 3));

  return boost::report_errors();
}
