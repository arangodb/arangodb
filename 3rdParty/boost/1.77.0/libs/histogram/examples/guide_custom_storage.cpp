// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_custom_storage

#include <algorithm> // std::for_each
#include <array>
#include <boost/histogram.hpp>
#include <boost/histogram/algorithm/sum.hpp>
#include <functional> // std::ref
#include <unordered_map>
#include <vector>

int main() {
  using namespace boost::histogram;
  const auto axis = axis::regular<>(10, 0.0, 1.0);

  auto data = {0.1, 0.3, 0.2, 0.7};

  // Create static histogram with vector<int> as counter storage, you can use
  // other arithmetic types as counters, e.g. double.
  auto h1 = make_histogram_with(std::vector<int>(), axis);
  std::for_each(data.begin(), data.end(), std::ref(h1));
  assert(algorithm::sum(h1) == 4);

  // Create static histogram with array<int, N> as counter storage which is
  // allocated completely on the stack (this is very fast). N may be larger than
  // the actual number of bins used; an exception is raised if N is too small to
  // hold all bins.
  auto h2 = make_histogram_with(std::array<int, 12>(), axis);
  std::for_each(data.begin(), data.end(), std::ref(h2));
  assert(algorithm::sum(h2) == 4);

  // Create static histogram with unordered_map as counter storage; this
  // generates a sparse histogram where only memory is allocated for bins that
  // are non-zero. This sounds like a good idea for high-dimensional histograms,
  // but maps come with a memory and run-time overhead. The default_storage
  // usually performs better in high dimensions.
  auto h3 = make_histogram_with(std::unordered_map<std::size_t, int>(), axis);
  std::for_each(data.begin(), data.end(), std::ref(h3));
  assert(algorithm::sum(h3) == 4);
}

//]
