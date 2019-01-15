//
// Copyright 2012 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_IO_GET_WRITER_HPP
#define BOOST_GIL_IO_GET_WRITER_HPP

#include <boost/gil/io/get_write_device.hpp>

namespace boost { namespace gil {

/// \brief Helper metafunction to generate writer type.
template< typename T
        , typename FormatTag
        , class Enable = void
        >
struct get_writer
{};


template< typename String
        , typename FormatTag
        >
struct get_writer< String
                 , FormatTag
                 , typename enable_if< mpl::and_< detail::is_supported_path_spec< String >
                                                , is_format_tag< FormatTag >
                                                >
                                     >::type
                 >
{
    typedef typename get_write_device< String
                                     , FormatTag
                                     >::type device_t;

    typedef writer< device_t
                  , FormatTag
                  > type;
};

template< typename Device
        , typename FormatTag
        >
struct get_writer< Device
                 , FormatTag
                 , typename enable_if< mpl::and_< detail::is_adaptable_output_device< FormatTag
                                                                                    , Device
                                                                                    >
                                                , is_format_tag< FormatTag >
                                                >
                                     >::type
                 >
{
    typedef typename get_write_device< Device
                                     , FormatTag
                                     >::type device_t;

    typedef writer< device_t
                  , FormatTag
                  > type;
};


/// \brief Helper metafunction to generate dynamic image writer type.
template< typename T
        , typename FormatTag
        , class Enable = void
        >
struct get_dynamic_image_writer
{};

template< typename String
        , typename FormatTag
        >
struct get_dynamic_image_writer< String
                               , FormatTag
                               , typename enable_if< mpl::and_< detail::is_supported_path_spec< String >
                                                              , is_format_tag< FormatTag >
                                                              >
                                                   >::type
                               >
{
    typedef typename get_write_device< String
                                     , FormatTag
                                     >::type device_t;

    typedef dynamic_image_writer< device_t
                                , FormatTag
                                > type;
};

template< typename Device
        , typename FormatTag
        >
struct get_dynamic_image_writer< Device
                               , FormatTag
                               , typename enable_if< mpl::and_< detail::is_write_device< FormatTag
                                                                                       , Device
                                                                                       >
                                                              , is_format_tag< FormatTag >
                                                              >
                                                   >::type
                               >
{
    typedef typename get_write_device< Device
                                     , FormatTag
                                     >::type device_t;

    typedef dynamic_image_writer< device_t
                                , FormatTag
                                > type;
};


} // namespace gil
} // namespace boost

#endif
