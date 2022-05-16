// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_histogram_projection

#include <boost/histogram.hpp>
#include <cassert>
#include <iostream>
#include <sstream>

int main() {
  using namespace boost::histogram;
  using namespace literals; // enables _c suffix

  // make a 2d histogram
  auto h = make_histogram(axis::regular<>(3, -1.0, 1.0), axis::integer<>(0, 2));

  h(-0.9, 0);
  h(0.9, 1);
  h(0.1, 0);

  auto hr0 = algorithm::project(h, 0_c); // keep only first axis
  auto hr1 = algorithm::project(h, 1_c); // keep only second axis

  // reduce does not remove counts; returned histograms are summed over
  // the removed axes, so h, hr0, and hr1 have same number of total counts;
  // we compute the sum of counts with the sum algorithm
  assert(algorithm::sum(h) == 3 && algorithm::sum(hr0) == 3 && algorithm::sum(hr1) == 3);

  std::ostringstream os1;
  for (auto&& x : indexed(h))
    os1 << "(" << x.index(0) << ", " << x.index(1) << "): " << *x << "\n";
  std::cout << os1.str() << std::flush;
  assert(os1.str() == "(0, 0): 1\n"
                      "(1, 0): 1\n"
                      "(2, 0): 0\n"
                      "(0, 1): 0\n"
                      "(1, 1): 0\n"
                      "(2, 1): 1\n");

  std::ostringstream os2;
  for (auto&& x : indexed(hr0)) os2 << "(" << x.index(0) << ", -): " << *x << "\n";
  std::cout << os2.str() << std::flush;
  assert(os2.str() == "(0, -): 1\n"
                      "(1, -): 1\n"
                      "(2, -): 1\n");

  std::ostringstream os3;
  for (auto&& x : indexed(hr1)) os3 << "(- ," << x.index(0) << "): " << *x << "\n";
  std::cout << os3.str() << std::flush;
  assert(os3.str() == "(- ,0): 2\n"
                      "(- ,1): 1\n");
}

//]
