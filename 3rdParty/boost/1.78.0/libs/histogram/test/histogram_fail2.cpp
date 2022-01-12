// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/histogram/axis/integer.hpp>
#include <boost/histogram/make_profile.hpp>

int main() {
  using namespace boost::histogram;

  struct accumulator {
    void operator()(double) {}
    void operator()(weight_type<double>, double) {}
  };

  auto h = make_histogram_with(dense_storage<accumulator>(), axis::integer<>(0, 5));

  // accumulator requires sample
  h(0, weight(1));

  auto values = {1, 2};
  h.fill(values, weight(1));
}
