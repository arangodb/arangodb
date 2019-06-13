//
// Copyright 2010 Kenneth Riddile
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_EXTENSION_IO_TARGA_DETAIL_SUPPORTED_TYPES_HPP
#define BOOST_GIL_EXTENSION_IO_TARGA_DETAIL_SUPPORTED_TYPES_HPP

#include <boost/gil/channel.hpp>
#include <boost/gil/color_base.hpp>
#include <boost/gil/io/base.hpp>

#include <boost/mpl/not.hpp>
#include <boost/type_traits/is_same.hpp>

namespace boost { namespace gil { namespace detail {

// Read support

template< typename Channel
        , typename ColorSpace
        >
struct targa_read_support : read_support_false
{
    static const targa_depth::type bpp = 0;
};

template<>
struct targa_read_support<uint8_t
                         , rgb_t
                         > : read_support_true
{
    static const targa_depth::type bpp = 24;
};


template<>
struct targa_read_support<uint8_t
                         , rgba_t
                         > : read_support_true
{
    static const targa_depth::type bpp = 32;
};


// Write support

template< typename Channel
        , typename ColorSpace
        >
struct targa_write_support : write_support_false
{};

template<>
struct targa_write_support<uint8_t
                          , rgb_t
                          > : write_support_true {};

template<>
struct targa_write_support<uint8_t
                          , rgba_t
                          > : write_support_true {};

} // namespace detail


template< typename Pixel >
struct is_read_supported< Pixel
                        , targa_tag
                        >
    : mpl::bool_< detail::targa_read_support< typename channel_type< Pixel >::type
                                            , typename color_space_type< Pixel >::type
                                            >::is_supported
                >
{
    typedef detail::targa_read_support< typename channel_type< Pixel >::type
                                      , typename color_space_type< Pixel >::type
                                      > parent_t;

    static const typename targa_depth::type bpp = parent_t::bpp;
};

template< typename Pixel >
struct is_write_supported< Pixel
                         , targa_tag
                         >
    : mpl::bool_< detail::targa_write_support< typename channel_type< Pixel >::type
                                             , typename color_space_type< Pixel >::type
                                             >::is_supported
                > {};

} // namespace gil
} // namespace boost


#endif
