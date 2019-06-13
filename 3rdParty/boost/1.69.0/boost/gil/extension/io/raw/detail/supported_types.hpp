//
// Copyright 2008 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_EXTENSION_IO_RAW_DETAIL_SUPPORTED_TYPES_HPP
#define BOOST_GIL_EXTENSION_IO_RAW_DETAIL_SUPPORTED_TYPES_HPP

#include <boost/gil/io/base.hpp>

#include <boost/gil/channel.hpp>
#include <boost/gil/color_base.hpp>

#include <boost/mpl/not.hpp>
#include <boost/type_traits/is_same.hpp>

namespace boost { namespace gil { namespace detail {

// Read support

template< typename Channel
        , typename ColorSpace
        >
struct raw_read_support : read_support_false {};

template<>
struct raw_read_support<uint8_t
                       , gray_t
                       > : read_support_true {};

template<>
struct raw_read_support<uint16_t
                       , gray_t
                       > : read_support_true {};

template<>
struct raw_read_support<uint8_t
                       , rgb_t
                       > : read_support_true {};

template<>
struct raw_read_support<uint16_t
                       , rgb_t
                       > : read_support_true {};

// Write support

struct raw_write_support : write_support_false {};

} // namespace detail

template< typename Pixel >
struct is_read_supported< Pixel,
                        raw_tag
                        >
    : mpl::bool_< detail::raw_read_support< typename channel_type< Pixel >::type
                                          , typename color_space_type< Pixel >::type
                                          >::is_supported > {};

template< typename Pixel >
struct is_write_supported< Pixel
                         , raw_tag
                         >
    : mpl::bool_< detail::raw_write_support::is_supported >
{};

} // namespace gil
} // namespace boost


#endif
