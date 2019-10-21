// Copyright 2015-2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_ACCUMULATORS_OSTREAM_HPP
#define BOOST_HISTOGRAM_ACCUMULATORS_OSTREAM_HPP

#include <boost/histogram/fwd.hpp>
#include <iosfwd>

/**
  \file boost/histogram/accumulators/ostream.hpp
  Simple streaming operators for the builtin accumulator types.

  The text representation is not guaranteed to be stable between versions of
  Boost.Histogram. This header is only included by
  [boost/histogram/ostream.hpp](histogram/reference.html#header.boost.histogram.ostream_hpp).
  To you use your own, include your own implementation instead of this header and do not
  include
  [boost/histogram/ostream.hpp](histogram/reference.html#header.boost.histogram.ostream_hpp).
 */

#ifndef BOOST_HISTOGRAM_DOXYGEN_INVOKED

namespace boost {
namespace histogram {
namespace accumulators {
template <class CharT, class Traits, class W>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,
                                              const sum<W>& x) {
  os << "sum(" << x.large() << " + " << x.small() << ")";
  return os;
}

template <class CharT, class Traits, class W>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,
                                              const weighted_sum<W>& x) {
  os << "weighted_sum(" << x.value() << ", " << x.variance() << ")";
  return os;
}

template <class CharT, class Traits, class W>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,
                                              const mean<W>& x) {
  os << "mean(" << x.count() << ", " << x.value() << ", " << x.variance() << ")";
  return os;
}

template <class CharT, class Traits, class W>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,
                                              const weighted_mean<W>& x) {
  os << "weighted_mean(" << x.sum_of_weights() << ", " << x.value() << ", "
     << x.variance() << ")";
  return os;
}

template <class CharT, class Traits, class T>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,
                                              const thread_safe<T>& x) {
  os << x.load();
  return os;
}
} // namespace accumulators
} // namespace histogram
} // namespace boost

#endif // BOOST_HISTOGRAM_DOXYGEN_INVOKED

#endif
