//
// Copyright 2005-2007 Adobe Systems Incorporated
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_CONCEPTS_DETAIL_TYPE_TRAITS_HPP
#define BOOST_GIL_CONCEPTS_DETAIL_TYPE_TRAITS_HPP

#include <boost/type_traits.hpp>

namespace boost { namespace gil { namespace detail {

template <typename T>
struct remove_const_and_reference
    : ::boost::remove_const<typename ::boost::remove_reference<T>::type>
{
};

}}} // namespace boost::gil::detail

#endif
