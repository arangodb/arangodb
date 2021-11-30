// Copyright 2020 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_histogram_in_api

#include <boost/histogram.hpp>
#include <cassert>

// function accepts any histogram and returns a copy
template <class Axes, class Storage>
boost::histogram::histogram<Axes, Storage> any_histogram(
    boost::histogram::histogram<Axes, Storage>& h) {
  return h;
}

// function only accepts histograms with fixed axis types and returns a copy
template <class Storage, class... Axes>
boost::histogram::histogram<std::tuple<Axes...>, Storage> only_static_histogram(
    boost::histogram::histogram<std::tuple<Axes...>, Storage>& h) {
  return h;
}

// function only accepts histograms with dynamic axis types and returns a copy
template <class Storage, class... Axes>
boost::histogram::histogram<std::vector<boost::histogram::axis::variant<Axes...>>,
                            Storage>
only_dynamic_histogram(
    boost::histogram::histogram<std::vector<boost::histogram::axis::variant<Axes...>>,
                                Storage>& h) {
  return h;
}

int main() {
  using namespace boost::histogram;

  auto histogram_with_static_axes = make_histogram(axis::regular<>(10, 0, 1));

  using axis_variant = axis::variant<axis::regular<>, axis::integer<>>;
  std::vector<axis_variant> axes;
  axes.emplace_back(axis::regular<>(5, 0, 1));
  axes.emplace_back(axis::integer<>(0, 1));
  auto histogram_with_dynamic_axes = make_histogram(axes);

  assert(any_histogram(histogram_with_static_axes) == histogram_with_static_axes);
  assert(any_histogram(histogram_with_dynamic_axes) == histogram_with_dynamic_axes);
  assert(only_static_histogram(histogram_with_static_axes) == histogram_with_static_axes);
  assert(only_dynamic_histogram(histogram_with_dynamic_axes) ==
         histogram_with_dynamic_axes);

  // does not compile: only_static_histogram(histogram_with_dynamic_axes)
  // does not compile: only_dynamic_histogram(histogram_with_static_axes)
}

//]
