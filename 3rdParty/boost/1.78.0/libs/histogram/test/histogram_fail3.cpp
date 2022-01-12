// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/histogram/axis/integer.hpp>
#include <boost/histogram/make_histogram.hpp>

int main() {
  struct accumulator {
    void operator()(double) {}
  };

  using namespace boost::histogram;
  auto h = make_histogram_with(dense_storage<accumulator>(), axis::integer<>(0, 5));

  // wrong number of sample arguments
  h(0, sample(1, 2));

  auto values = {0, 1};
  h.fill(values, sample(values, values));
}
