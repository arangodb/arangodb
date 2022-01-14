// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/histogram/axis.hpp>
#include <iostream>

#define SHOW_SIZE(x) std::cout << #x << " " << sizeof(x) << std::endl

int main() {
  using namespace boost::histogram;

  using regular = axis::regular<>;
  using regular_float = axis::regular<float>;
  using regular_pow = axis::regular<double, axis::transform::pow>;
  using regular_no_metadata = axis::regular<double, axis::transform::id, axis::null_type>;
  using circular = axis::circular<>;
  using variable = axis::variable<>;
  using integer = axis::integer<>;
  using category = axis::category<>;
  using boolean = axis::boolean<>;
  using boolean_no_metadata = axis::boolean<axis::null_type>;
  using variant = axis::variant<regular, circular, variable, integer, category, boolean>;

  SHOW_SIZE(regular);
  SHOW_SIZE(regular_float);
  SHOW_SIZE(regular_pow);
  SHOW_SIZE(regular_no_metadata);
  SHOW_SIZE(circular);
  SHOW_SIZE(variable);
  SHOW_SIZE(integer);
  SHOW_SIZE(category);
  SHOW_SIZE(boolean);
  SHOW_SIZE(boolean_no_metadata);
  SHOW_SIZE(variant);
}
