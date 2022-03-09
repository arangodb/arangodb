// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_custom_accumulators_ouroboros

#include <boost/histogram.hpp>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>

int main() {
  using namespace boost::histogram;

  // First we define the nested histogram type.
  using axis_t = axis::category<int, axis::null_type, axis::option::growth_t>;
  using base_t = histogram<std::tuple<axis_t>>;

  // Now we make an accumulator out of it by using inheritance.
  // We only need to implement operator(). A matching version of operator() is actually
  // present in base_t, but it is templated and this is not allowed by the accumulator
  // concept. Initialization could also happen here. We don't need to initialize anything
  // here, because the default constructor of base_t is called automatically and
  // sufficient for this example.
  struct hist_t : base_t {
    void operator()(const double x) { base_t::operator()(x); }
  };

  auto h = make_histogram_with(dense_storage<hist_t>(), axis::integer<>(1, 4));

  auto x = {1, 1, 2, 2};
  auto s = {1, 2, 3, 3}; // samples are filled into the nested histograms
  h.fill(x, sample(s));

  std::ostringstream os;
  for (auto&& x : indexed(h)) {
    os << x.bin() << " ";
    for (auto&& y : indexed(*x)) { os << "(" << y.bin() << ": " << *y << ") "; }
    os << "\n";
  }

  std::cout << os.str() << std::flush;
  assert(os.str() == "1 (1: 1) (2: 1) \n"
                     "2 (3: 2) \n"
                     "3 \n");
}

//]
