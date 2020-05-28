// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_DETAIL_NOOP_MUTEX_HPP
#define BOOST_HISTOGRAM_DETAIL_NOOP_MUTEX_HPP

namespace boost {
namespace histogram {
namespace detail {

struct noop_mutex {
  bool try_lock() noexcept { return true; }
  void lock() noexcept {}
  void unlock() noexcept {}
};

} // namespace detail
} // namespace histogram
} // namespace boost

#endif
