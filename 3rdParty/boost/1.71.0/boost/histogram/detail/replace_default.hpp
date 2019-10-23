// Copyright 2015-2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_DETAIL_REPLACE_DEFAULT_HPP
#define BOOST_HISTOGRAM_DETAIL_REPLACE_DEFAULT_HPP

#include <boost/core/use_default.hpp>
#include <type_traits>

namespace boost {
namespace histogram {
namespace detail {

template <class T, class Default>
using replace_default =
    std::conditional_t<std::is_same<T, boost::use_default>::value, Default, T>;

} // namespace detail
} // namespace histogram
} // namespace boost

#endif
