// Copyright 2015-2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_DETAIL_MAKE_DEFAULT_HPP
#define BOOST_HISTOGRAM_DETAIL_MAKE_DEFAULT_HPP

#include <boost/histogram/detail/detect.hpp>
#include <boost/histogram/detail/static_if.hpp>

namespace boost {
namespace histogram {
namespace detail {

template <class T>
auto make_default(const T& t) {
  return static_if<has_allocator<T>>([](const auto& t) { return T(t.get_allocator()); },
                                     [](const auto&) { return T{}; }, t);
}

} // namespace detail
} // namespace histogram
} // namespace boost

#endif
