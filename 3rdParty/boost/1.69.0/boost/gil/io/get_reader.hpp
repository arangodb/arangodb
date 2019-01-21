//
// Copyright 2012 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_IO_GET_READER_HPP
#define BOOST_GIL_IO_GET_READER_HPP

#include <boost/gil/io/get_read_device.hpp>

namespace boost { namespace gil {

/// \brief Helper metafunction to generate image reader type.
template< typename T
        , typename FormatTag
        , typename ConversionPolicy
        , class Enable = void
        >
struct get_reader
{};

template< typename String
        , typename FormatTag
        , typename ConversionPolicy
        >
struct get_reader< String
                 , FormatTag
                 , ConversionPolicy
                 , typename enable_if< mpl::and_< detail::is_supported_path_spec< String >
                                                , is_format_tag< FormatTag >
                                                >
                                     >::type
                 >
{
    typedef typename get_read_device< String
                                    , FormatTag
                                    >::type device_t;

    typedef reader< device_t
                  , FormatTag
                  , ConversionPolicy
                  > type;
};

template< typename Device
        , typename FormatTag
        , typename ConversionPolicy
        >
struct get_reader< Device
                 , FormatTag
                 , ConversionPolicy
                 , typename enable_if< mpl::and_< detail::is_adaptable_input_device< FormatTag
                                                                                   , Device
                                                                                   >
                                                , is_format_tag< FormatTag >
                                                >
                                     >::type
                 >
{
    typedef typename get_read_device< Device
                                    , FormatTag
                                    >::type device_t;

    typedef reader< device_t
                  , FormatTag
                  , ConversionPolicy
                  > type;
};


/// \brief Helper metafunction to generate dynamic image reader type.
template< typename T
        , typename FormatTag
        , class Enable = void
        >
struct get_dynamic_image_reader
{};

template< typename String
        , typename FormatTag
        >
struct get_dynamic_image_reader< String
                               , FormatTag
                               , typename enable_if< mpl::and_< detail::is_supported_path_spec< String >
                                                              , is_format_tag< FormatTag >
                                                              >
                                                   >::type
                               >
{
    typedef typename get_read_device< String
                                    , FormatTag
                                    >::type device_t;

    typedef dynamic_image_reader< device_t
                                , FormatTag
                                > type;
};

template< typename Device
        , typename FormatTag
        >
struct get_dynamic_image_reader< Device
                               , FormatTag
                               , typename enable_if< mpl::and_< detail::is_adaptable_input_device< FormatTag
                                                                                                 , Device
                                                                                                 >
                                                              , is_format_tag< FormatTag >
                                                              >
                                                   >::type
                               >
{
    typedef typename get_read_device< Device
                                    , FormatTag
                                    >::type device_t;

    typedef dynamic_image_reader< device_t
                                , FormatTag
                                > type;
};


/////////////////////////////////////////////////////////////

/// \brief Helper metafunction to generate image backend type.
template< typename T
        , typename FormatTag
        , class Enable = void
        >
struct get_reader_backend
{};

template< typename String
        , typename FormatTag
        >
struct get_reader_backend< String
                         , FormatTag
                         , typename enable_if< mpl::and_< detail::is_supported_path_spec< String >
                                                        , is_format_tag< FormatTag >
                                                        >
                                             >::type
                         >
{
    typedef typename get_read_device< String
                                    , FormatTag
                                    >::type device_t;

    typedef reader_backend< device_t
                          , FormatTag
                          > type;
};

template< typename Device
        , typename FormatTag
        >
struct get_reader_backend< Device
                         , FormatTag
                         , typename enable_if< mpl::and_< detail::is_adaptable_input_device< FormatTag
                                                                                           , Device
                                                                                           >
                                                        , is_format_tag< FormatTag >
                                                        >
                                             >::type
                         >
{
    typedef typename get_read_device< Device
                                    , FormatTag
                                    >::type device_t;

    typedef reader_backend< device_t
                          , FormatTag
                          > type;
};

/// \brief Helper metafunction to generate image scanline_reader type.
template< typename T
        , typename FormatTag
        >
struct get_scanline_reader
{
    typedef typename get_read_device< T
                                    , FormatTag
                                    >::type device_t;

    typedef scanline_reader< device_t
                           , FormatTag
                           > type;
};

} // namespace gil
} // namespace boost

#endif
