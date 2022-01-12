// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_stdlib_algorithms

#include <boost/histogram.hpp>
#include <cassert>

#include <algorithm> // fill, any_of, min_element, max_element
#include <cmath>     // sqrt
#include <numeric>   // partial_sum, inner_product

int main() {
  using namespace boost::histogram;

  // make histogram that represents a probability density function (PDF)
  auto h1 = make_histogram(axis::regular<>(4, 1.0, 3.0));

  // make indexed range to skip underflow and overflow cells
  auto ind = indexed(h1);

  // use std::fill to set all counters to 0.25 (except under- and overflow counters)
  std::fill(ind.begin(), ind.end(), 0.25);

  // compute the cumulative density function (CDF), overriding cell values
  std::partial_sum(ind.begin(), ind.end(), ind.begin());

  assert(h1.at(-1) == 0.00);
  assert(h1.at(0) == 0.25);
  assert(h1.at(1) == 0.50);
  assert(h1.at(2) == 0.75);
  assert(h1.at(3) == 1.00);
  assert(h1.at(4) == 0.00);

  // use any_of to check if any cell values are smaller than 0.1,
  auto b = std::any_of(ind.begin(), ind.end(), [](const auto& x) { return x < 0.1; });
  assert(b == false); // under- and overflow cells are zero, but skipped

  // find minimum element
  auto min_it = std::min_element(ind.begin(), ind.end());
  assert(*min_it == 0.25); // under- and overflow cells are skipped

  // find maximum element
  auto max_it = std::max_element(ind.begin(), ind.end());
  assert(*max_it == 1.0);

  // make second PDF
  auto h2 = make_histogram(axis::regular<>(4, 1.0, 4.0));
  h2.at(0) = 0.1;
  h2.at(1) = 0.3;
  h2.at(2) = 0.2;
  h2.at(3) = 0.4;

  // computing cosine similiarity: cos(theta) = A dot B / sqrt((A dot A) * (B dot B))
  auto ind2 = indexed(h2);
  const auto aa = std::inner_product(ind.begin(), ind.end(), ind.begin(), 0.0);
  const auto bb = std::inner_product(ind2.begin(), ind2.end(), ind2.begin(), 0.0);
  const auto ab = std::inner_product(ind.begin(), ind.end(), ind2.begin(), 0.0);
  const auto cos_sim = ab / std::sqrt(aa * bb);

  assert(std::abs(cos_sim - 0.967) < 1e-2);
}

//]
