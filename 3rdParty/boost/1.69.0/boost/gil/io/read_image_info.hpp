//
// Copyright 2007-2012 Christian Henning, Andreas Pokorny, Lubomir Bourdev
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_IO_READ_IMAGE_INFO_HPP
#define BOOST_GIL_IO_READ_IMAGE_INFO_HPP

#include <boost/gil/io/base.hpp>
#include <boost/gil/io/device.hpp>
#include <boost/gil/io/get_reader.hpp>
#include <boost/gil/io/path_spec.hpp>

#include <boost/mpl/and.hpp>
#include <boost/type_traits/is_base_and_derived.hpp>
#include <boost/utility/enable_if.hpp>

namespace boost{ namespace gil {

/// \ingroup IO

/// \brief Returns the image format backend. Backend is format specific.
/// \param file      It's a device. Must satisfy is_adaptable_input_device metafunction.
/// \param settings  Specifies read settings depending on the image format.
/// \return image_read_info object dependent on the image format.
/// \throw std::ios_base::failure
template< typename Device
        , typename FormatTag
        >
inline
typename get_reader_backend< Device
                           , FormatTag
                           >::type
read_image_info( Device&                                 file
               , const image_read_settings< FormatTag >& settings
               , typename enable_if< mpl::and_< detail::is_adaptable_input_device< FormatTag
                                                                                 , Device
                                                                                 >
                                              , is_format_tag< FormatTag >
                                              >
                                   >::type* /* ptr */ = 0
               )
{
    return make_reader_backend( file, settings );
}

/// \brief Returns the image format backend. Backend is format specific.
/// \param file It's a device. Must satisfy is_adaptable_input_device metafunction.
/// \param tag  Defines the image format. Must satisfy is_format_tag metafunction.
/// \return image_read_info object dependent on the image format.
/// \throw std::ios_base::failure
template< typename Device
        , typename FormatTag
        >
inline
typename get_reader_backend< Device
                           , FormatTag
                           >::type
read_image_info( Device&          file
               , const FormatTag&
               , typename enable_if< mpl::and_< detail::is_adaptable_input_device< FormatTag
                                                                                 , Device
                                                                                 >
                                              , is_format_tag< FormatTag >
                                              >
                                   >::type* /* ptr */ = 0
               )
{
    return read_image_info( file
                          , image_read_settings< FormatTag >()
                          );
}

/// \brief Returns the image format backend. Backend is format specific.
/// \param file_name File name. Must satisfy is_supported_path_spec metafunction.
/// \param settings  Specifies read settings depending on the image format.
/// \return image_read_info object dependent on the image format.
/// \throw std::ios_base::failure
template< typename String
        , typename FormatTag
        >
inline
typename get_reader_backend< String
                           , FormatTag
                           >::type
read_image_info( const String&                           file_name
               , const image_read_settings< FormatTag >& settings
               , typename enable_if< mpl::and_< is_format_tag< FormatTag >
                                              , detail::is_supported_path_spec< String >
                                              >
                                   >::type* /* ptr */ = 0
               )
{
    return make_reader_backend( file_name, settings );
}

/// \brief Returns the image format backend. Backend is format specific.
/// \param file_name File name. Must satisfy is_supported_path_spec metafunction.
/// \param tag       Defines the image format. Must satisfy is_format_tag metafunction.
/// \return image_read_info object dependent on the image format.
/// \throw std::ios_base::failure
template< typename String
        , typename FormatTag
        >
inline
typename get_reader_backend< String
                           , FormatTag
                           >::type
read_image_info( const String&    file_name
               , const FormatTag&
               , typename enable_if< mpl::and_< is_format_tag< FormatTag >
                                              , detail::is_supported_path_spec< String >
                                              >
                                   >::type* /* ptr */ = 0
               )
{
    return read_image_info( file_name
                          , image_read_settings< FormatTag >()
                          );
}

} // namespace gil
} // namespace boost

#endif
