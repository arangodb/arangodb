// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_DETAIL_ARGS_TYPE_HPP
#define BOOST_HISTOGRAM_DETAIL_ARGS_TYPE_HPP

#include <boost/config/workaround.hpp>
#if BOOST_WORKAROUND(BOOST_GCC, >= 60000)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnoexcept-type"
#endif
#include <boost/callable_traits/args.hpp>
#if BOOST_WORKAROUND(BOOST_GCC, >= 60000)
#pragma GCC diagnostic pop
#endif
#include <boost/mp11/list.hpp> // mp_pop_front
#include <tuple>
#include <type_traits> // is_member_function_pointer

namespace boost {
namespace histogram {
namespace detail {

template <class T, class Args = boost::callable_traits::args_t<T>>
using args_type = std::conditional_t<std::is_member_function_pointer<T>::value,
                                     mp11::mp_pop_front<Args>, Args>;

template <class T, std::size_t N = 0>
using arg_type = std::tuple_element_t<N, args_type<T>>;

} // namespace detail
} // namespace histogram
} // namespace boost

#endif
