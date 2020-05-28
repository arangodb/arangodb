// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_make_dynamic_histogram

#include <boost/histogram.hpp>
#include <cassert>
#include <sstream>
#include <vector>

const char* config = "4 1.0 2.0\n"
                     "5 3.0 4.0\n";

int main() {
  using namespace boost::histogram;

  // read axis config from a config file (mocked here with std::istringstream)
  // and create vector of regular axes, the number of axis is not known at compile-time
  std::istringstream is(config);
  auto v1 = std::vector<axis::regular<>>();
  while (is.good()) {
    unsigned bins;
    double start, stop;
    is >> bins >> start >> stop;
    v1.emplace_back(bins, start, stop);
  }

  // create histogram from iterator range
  // (copying or moving the vector also works, move is shown below)
  auto h1 = make_histogram(v1.begin(), v1.end());
  assert(h1.rank() == v1.size());

  // with a vector of axis::variant (polymorphic axis type that can hold any one of the
  // template arguments at a time) the types and number of axis can vary at run-time
  auto v2 = std::vector<axis::variant<axis::regular<>, axis::integer<>>>();
  v2.emplace_back(axis::regular<>(100, -1.0, 1.0));
  v2.emplace_back(axis::integer<>(1, 7));

  // create dynamic histogram by moving the vector
  auto h2 = make_histogram(std::move(v2));
  assert(h2.rank() == 2);
}

//]
