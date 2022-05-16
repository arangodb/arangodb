// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_custom_accumulators_with_metadata

#include <boost/histogram.hpp>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>

int main() {
  using namespace boost::histogram;

  // derive custom accumulator from one of the builtins
  struct accumulator_with_metadata : accumulators::count<> {
    std::string meta; // custom meta data

    // arbitrary additional data and interface could be added here
  };

  // make 1D histogram with custom accmulator
  auto h = make_histogram_with(dense_storage<accumulator_with_metadata>(),
                               axis::integer<>(1, 4));

  // fill some weighted entries
  auto x = {1, 0, 2, 1};
  h.fill(x);

  // assigning meta data to two bins
  h[0].meta = "Foo";
  h[2].meta = "Bar";

  std::ostringstream os;
  for (auto&& x : indexed(h))
    os << x.bin() << " value " << x->value() << " meta " << x->meta << "\n";

  std::cout << os.str() << std::flush;
  assert(os.str() == "1 value 2 meta Foo\n"
                     "2 value 1 meta \n"
                     "3 value 0 meta Bar\n");
}

//]
