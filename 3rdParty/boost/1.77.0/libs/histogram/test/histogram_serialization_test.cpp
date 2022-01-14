// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <cassert>
#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/axis.hpp>
#include <boost/histogram/axis/ostream.hpp>
#include <boost/histogram/ostream.hpp>
#include <boost/histogram/serialization.hpp>
#include <cmath>
#include <string>
#include "throw_exception.hpp"
#include "utility_histogram.hpp"
#include "utility_serialization.hpp"

using namespace boost::histogram;

template <typename Tag>
void run_tests(const std::string& filename) {
  // histogram_serialization
  namespace tr = axis::transform;
  using def = use_default;
  using axis::option::none_t;
  auto a =
      make(Tag(), axis::regular<double, def, def, none_t>(1, -1, 1, "reg"),
           axis::circular<float, def, none_t>(1, 0.0, 1.0, "cir"),
           axis::regular<double, tr::log, def, none_t>(1, 1, std::exp(2), "reg-log"),
           axis::regular<double, tr::pow, std::vector<int>, axis::option::overflow_t>(
               tr::pow(0.5), 1, 1, 100, {1, 2, 3}),
           axis::variable<double, def, none_t>({1.5, 2.5}, "var"),
           axis::category<int, def, none_t>{3, 1},
           axis::integer<int, axis::null_type, none_t>(1, 2));
  a(0.5, 0.2, 2, 20, 2.2, 1, 1);
  print_xml(filename, a);

  auto b = decltype(a)();
  BOOST_TEST_NE(a, b);
  load_xml(filename, b);
  BOOST_TEST_EQ(a, b);
}

int main(int argc, char** argv) {
  assert(argc == 2);
  run_tests<static_tag>(join(argv[1], "histogram_serialization_test_static.xml"));
  run_tests<dynamic_tag>(join(argv[1], "histogram_serialization_test_dynamic.xml"));
  return boost::report_errors();
}
