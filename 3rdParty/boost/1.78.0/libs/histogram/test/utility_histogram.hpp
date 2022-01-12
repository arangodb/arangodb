// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_TEST_UTILITY_HISTOGRAM_HPP
#define BOOST_HISTOGRAM_TEST_UTILITY_HISTOGRAM_HPP

#include <boost/histogram/axis/category.hpp>
#include <boost/histogram/axis/integer.hpp>
#include <boost/histogram/axis/regular.hpp>
#include <boost/histogram/axis/variable.hpp>
#include <boost/histogram/axis/variant.hpp>
#include <boost/histogram/make_histogram.hpp>
#include <boost/mp11/algorithm.hpp>
#include <type_traits>
#include <vector>

namespace boost {
namespace histogram {

template <class... Ts>
auto make_axis_vector(const Ts&... ts) {
  // make sure the variant is never trivial (contains only one type)
  using R = axis::regular<double, boost::use_default, axis::null_type>;
  using I = axis::integer<int, axis::null_type, axis::option::none_t>;
  using V = axis::variable<double, axis::null_type>;
  using C = axis::category<int, axis::null_type>;
  using Var = boost::mp11::mp_unique<axis::variant<Ts..., R, I, V, C>>;
  return std::vector<Var>({Var(ts)...});
}

struct static_tag : std::false_type {};
struct dynamic_tag : std::true_type {};

template <class... Axes>
auto make(static_tag, const Axes&... axes) {
  return make_histogram(axes...);
}

template <class S, class... Axes>
auto make_s(static_tag, S&& s, const Axes&... axes) {
  return make_histogram_with(s, axes...);
}

template <class... Axes>
auto make(dynamic_tag, const Axes&... axes) {
  return make_histogram(make_axis_vector(axes...));
}

template <class S, class... Axes>
auto make_s(dynamic_tag, S&& s, const Axes&... axes) {
  return make_histogram_with(s, make_axis_vector(axes...));
}

} // namespace histogram
} // namespace boost

#endif
