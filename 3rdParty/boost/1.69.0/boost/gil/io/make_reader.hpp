//
// Copyright 2012 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_IO_MAKE_READER_HPP
#define BOOST_GIL_IO_MAKE_READER_HPP

#include <boost/gil/io/get_reader.hpp>

#include <boost/utility/enable_if.hpp>

namespace boost { namespace gil {

template< typename String
        , typename FormatTag
        , typename ConversionPolicy
        >
inline
typename get_reader< String
                   , FormatTag
                   , ConversionPolicy
                   >::type
make_reader( const String&    file_name
           , const image_read_settings< FormatTag >& settings
           , const ConversionPolicy&
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

    return typename get_reader< String
                              , FormatTag
                              , ConversionPolicy
                              >::type( device
                                     , settings
                                     );
}

template< typename FormatTag
        , typename ConversionPolicy
        >
inline
typename get_reader< std::wstring
                   , FormatTag
                   , ConversionPolicy
                   >::type
make_reader( const std::wstring& file_name
           , const image_read_settings< FormatTag >& settings
           , const ConversionPolicy&
           )
{
    const char* str = detail::convert_to_native_string( file_name );

    typename get_read_device< std::wstring
                            , FormatTag
                            >::type device( str
                                          , typename detail::file_stream_device< FormatTag >::read_tag()
                                          );

    delete[] str;

    return typename get_reader< std::wstring
                              , FormatTag
                              , ConversionPolicy
                              >::type( device
                                     , settings
                                     );
}

#ifdef BOOST_GIL_IO_ADD_FS_PATH_SUPPORT
template< typename FormatTag
        , typename ConversionPolicy
        >
inline
typename get_reader< std::wstring
                   , FormatTag
                   , ConversionPolicy
                   >::type
make_reader( const filesystem::path&                 path
           , const image_read_settings< FormatTag >& settings
           , const ConversionPolicy&                 cc
           )
{
    return make_reader( path.wstring()
                      , settings
                      , cc
                      );
}
#endif // BOOST_GIL_IO_ADD_FS_PATH_SUPPORT

template< typename Device
        , typename FormatTag
        , typename ConversionPolicy
        >
inline
typename get_reader< Device
                   , FormatTag
                   , ConversionPolicy
                   >::type
make_reader( Device&                                 file
           , const image_read_settings< FormatTag >& settings
           , const ConversionPolicy&
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

    return typename get_reader< Device
                              , FormatTag
                              , ConversionPolicy
                              >::type( device
                                     , settings
                                     );
}

// no image_read_settings

template< typename String
        , typename FormatTag
        , typename ConversionPolicy
        >
inline
typename get_reader< String
                   , FormatTag
                   , ConversionPolicy
                   >::type
make_reader( const String&    file_name
           , const FormatTag&
           , const ConversionPolicy& cc
           , typename enable_if< mpl::and_< detail::is_supported_path_spec< String >
                                          , is_format_tag< FormatTag >
                                                   >
                               >::type* /* ptr */ = 0
           )
{
    return make_reader( file_name
                      , image_read_settings< FormatTag >()
                      , cc
                      );
}

template< typename FormatTag
        , typename ConversionPolicy
        >
inline
typename get_reader< std::wstring
                   , FormatTag
                   , ConversionPolicy
                   >::type
make_reader( const std::wstring&     file_name
           , const FormatTag&
           , const ConversionPolicy& cc
           )
{
    return make_reader( file_name
                      , image_read_settings< FormatTag >()
                      , cc
                      );
}

#ifdef BOOST_GIL_IO_ADD_FS_PATH_SUPPORT
template< typename FormatTag
        , typename ConversionPolicy
        >
inline
typename get_reader< std::wstring
                   , FormatTag
                   , ConversionPolicy
                   >::type
make_reader( const filesystem::path& path
           , const FormatTag&
           , const ConversionPolicy& cc
           )
{
    return make_reader( path.wstring()
                      , image_read_settings< FormatTag >()
                      , cc
                      );
}
#endif // BOOST_GIL_IO_ADD_FS_PATH_SUPPORT

template< typename Device
        , typename FormatTag
        , typename ConversionPolicy
        >
inline
typename get_reader< Device
                   , FormatTag
                   , ConversionPolicy
                   >::type
make_reader( Device&                 file
           , const FormatTag&
           , const ConversionPolicy& cc
           , typename enable_if< mpl::and_< detail::is_adaptable_input_device< FormatTag
                                                                             , Device
                                                                             >
                                          , is_format_tag< FormatTag >
                                          >
                               >::type* /* ptr */ = 0
           )
{
    return make_reader( file
                      , image_read_settings< FormatTag >()
                      , cc
                      );
}

} // namespace gil
} // namespace boost

#endif
