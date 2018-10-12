/*
    Copyright 2012 Christian Henning
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_IO_MAKE_DYNAMIC_IMAGE_READER_HPP
#define BOOST_GIL_IO_MAKE_DYNAMIC_IMAGE_READER_HPP

////////////////////////////////////////////////////////////////////////////////////////
/// \file
/// \brief
/// \author Christian Henning \n
///
/// \date 2012 \n
///
////////////////////////////////////////////////////////////////////////////////////////

#include <boost/utility/enable_if.hpp>

#include <boost/gil/io/get_reader.hpp>

namespace boost { namespace gil {

template< typename String
        , typename FormatTag
        >
inline
typename get_dynamic_image_reader< String
                                 , FormatTag
                                 >::type
make_dynamic_image_reader( const String&    file_name
                         , const image_read_settings< FormatTag >& settings
                         , typename enable_if< mpl::and_< detail::is_supported_path_spec< String >
                                                        , is_format_tag< FormatTag >
                                                                 >
                                             >::type* /* ptr */ = 0
                         )
{
    typename get_read_device< String
                   , FormatTag
                   >::type device( detail::convert_to_native_string( file_name )
                                 , typename detail::file_stream_device< FormatTag >::read_tag()
                                 );

    return typename get_dynamic_image_reader< String
                                            , FormatTag
                                            >::type( device
                                                   , settings
                                                   );
}

template< typename FormatTag >
inline
typename get_dynamic_image_reader< std::wstring
                                 , FormatTag
                                 >::type
make_dynamic_image_reader( const std::wstring& file_name
                         , const image_read_settings< FormatTag >& settings
                         )
{
    const char* str = detail::convert_to_native_string( file_name );

    typename get_read_device< std::wstring
                   , FormatTag
                   >::type device( str
                                 , typename detail::file_stream_device< FormatTag >::read_tag()
                                 );

    delete[] str;

    return typename get_dynamic_image_reader< std::wstring
                                            , FormatTag
                                            >::type( device
                                                   , settings
                                                   );
}

#ifdef BOOST_GIL_IO_ADD_FS_PATH_SUPPORT
template< typename FormatTag >
inline
typename get_dynamic_image_reader< std::wstring
                                 , FormatTag
                                 >::type
make_dynamic_image_reader( const filesystem::path&                 path
                         , const image_read_settings< FormatTag >& settings
                         )
{
    return make_dynamic_image_reader( path.wstring()
                                    , settings
                                    );
}
#endif // BOOST_GIL_IO_ADD_FS_PATH_SUPPORT

template< typename Device
        , typename FormatTag
        >
inline
typename get_dynamic_image_reader< Device
                                 , FormatTag
                                 >::type
make_dynamic_image_reader( Device&          file
                         , const image_read_settings< FormatTag >& settings
                         , typename enable_if< mpl::and_< detail::is_adaptable_input_device< FormatTag
                                                                                           , Device
                                                                                           >
                                                        , is_format_tag< FormatTag >
                                                        >
                                             >::type* /* ptr */ = 0
                         )
{
    typename get_read_device< Device
                   , FormatTag
                   >::type device( file );

    return typename get_dynamic_image_reader< Device
                                            , FormatTag
                                            >::type( device
                                                   , settings
                                                   );
}

// without image_read_settings

template< typename String
        , typename FormatTag
        >
inline
typename get_dynamic_image_reader< String
                                 , FormatTag
                                 >::type
make_dynamic_image_reader( const String&    file_name
                         , const FormatTag&
                         , typename enable_if< mpl::and_< detail::is_supported_path_spec< String >
                                                        , is_format_tag< FormatTag >
                                                                 >
                                             >::type* /* ptr */ = 0
                         )
{
    return make_dynamic_image_reader( file_name
                                    , image_read_settings< FormatTag >()
                                    );
}

template< typename FormatTag >
inline
typename get_dynamic_image_reader< std::wstring
                                 , FormatTag
                                 >::type
make_dynamic_image_reader( const std::wstring& file_name
                         , const FormatTag&
                         )
{
    return make_dynamic_image_reader( file_name
                                    , image_read_settings< FormatTag >()
                                    );
}

#ifdef BOOST_GIL_IO_ADD_FS_PATH_SUPPORT
template< typename FormatTag >
inline
typename get_dynamic_image_reader< std::wstring
                                 , FormatTag
                                 >::type
make_dynamic_image_reader( const filesystem::path& path
                         , const FormatTag&
                         )
{
    return make_dynamic_image_reader( path.wstring()
                                    , image_read_settings< FormatTag >()
                                    );
}
#endif // BOOST_GIL_IO_ADD_FS_PATH_SUPPORT

template< typename Device
        , typename FormatTag
        >
inline
typename get_dynamic_image_reader< Device
                                 , FormatTag
                                 >::type
make_dynamic_image_reader( Device&          file
                         , const FormatTag&
                         , typename enable_if< mpl::and_< detail::is_adaptable_input_device< FormatTag
                                                                                           , Device
                                                                                           >
                                                        , is_format_tag< FormatTag >
                                                        >
                                             >::type* /* ptr */ = 0
                         )
{
    return make_dynamic_image_reader( file
                                    , image_read_settings< FormatTag >()
                                    );
}

} // namespace gil
} // namespace boost

#endif
