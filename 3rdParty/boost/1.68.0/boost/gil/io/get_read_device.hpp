/*
    Copyright 2012 Christian Henning
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_IO_GET_READ_DEVICE_HPP
#define BOOST_GIL_IO_GET_READ_DEVICE_HPP

////////////////////////////////////////////////////////////////////////////////////////
/// \file
/// \brief
/// \author Christian Henning \n
///
/// \date 2012 \n
///
////////////////////////////////////////////////////////////////////////////////////////

#include <boost/mpl/and.hpp>
#include <boost/utility/enable_if.hpp>

#include <boost/gil/io/device.hpp>
#include <boost/gil/io/path_spec.hpp>

namespace boost { namespace gil {

template< typename T
        , typename FormatTag
        , class Enable = void
        >
struct get_read_device
{};

template< typename Device
        , typename FormatTag
        >
struct get_read_device< Device
                      , FormatTag
                      , typename enable_if< mpl::and_< detail::is_adaptable_input_device< FormatTag
                                                                                        , Device
                                                                                        >
                                                     , is_format_tag< FormatTag >
                                                     >
                                          >::type
                 >
{
    typedef typename detail::is_adaptable_input_device< FormatTag
                                                      , Device
                                                      >::device_type type;
};

template< typename String
        , typename FormatTag
        >
struct get_read_device< String
                      , FormatTag
                      , typename enable_if< mpl::and_< detail::is_supported_path_spec< String >
                                                     , is_format_tag< FormatTag >
                                                     >
                                          >::type
                      >
{
    typedef detail::file_stream_device< FormatTag > type;
};

} // namespace gil
} // namespace boost

#endif
