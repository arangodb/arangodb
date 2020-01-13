// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_ALGORITHM_SUM_HPP
#define BOOST_HISTOGRAM_ALGORITHM_SUM_HPP

#include <boost/histogram/accumulators/sum.hpp>
#include <boost/histogram/fwd.hpp>
#include <boost/mp11/utility.hpp>
#include <numeric>
#include <type_traits>

namespace boost {
namespace histogram {
namespace algorithm {
/** Compute the sum over all histogram cells, including underflow/overflow bins.

  If the value type of the histogram is an integral or floating point type,
  boost::accumulators::sum<double> is used to compute the sum, else the original value
  type is used. Compilation fails, if the value type does not support operator+=.

  Return type is double if the value type of the histogram is integral or floating point,
  and the original value type otherwise.
 */
template <class A, class S>
auto sum(const histogram<A, S>& h) {
  using T = typename histogram<A, S>::value_type;
  using Sum = mp11::mp_if<std::is_arithmetic<T>, accumulators::sum<double>, T>;
  Sum sum;
  for (auto&& x : h) sum += x;
  using R = mp11::mp_if<std::is_arithmetic<T>, double, T>;
  return static_cast<R>(sum);
}
} // namespace algorithm
} // namespace histogram
} // namespace boost

#endif
