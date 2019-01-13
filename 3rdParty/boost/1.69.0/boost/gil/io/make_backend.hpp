//
//
// Copyright 2012 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_IO_MAKE_BACKEND_HPP
#define BOOST_GIL_IO_MAKE_BACKEND_HPP

#include <boost/gil/io/get_reader.hpp>

#include <boost/utility/enable_if.hpp>

namespace boost { namespace gil {

template< typename String
        , typename FormatTag
        >
inline
typename get_reader_backend< String
                           , FormatTag
                           >::type
make_reader_backend( const String&                           file_name
                   , const image_read_settings< FormatTag >& settings
                   , typename enable_if< mpl::and_< detail::is_supported_path_spec< String >
                                                  , is_format_tag< FormatTag >
                                                           >
                                       >::type* /* ptr */ = 0
                   )
{
    typedef typename get_read_device< String
                                    , FormatTag
                                    >::type device_t;

    device_t device( detail::convert_to_native_string( file_name )
                   , typename detail::file_stream_device< FormatTag >::read_tag()
                   );

    return reader_backend< device_t, FormatTag >( device, settings );
}

template< typename FormatTag >
inline
typename get_reader_backend< std::wstring
                           , FormatTag
                           >::type
make_reader_backend( const std::wstring&                     file_name
                   , const image_read_settings< FormatTag >& settings
                   )
{
    typedef typename get_read_device< std::wstring
                                    , FormatTag
                                    >::type device_t;

    const char* str = detail::convert_to_native_string( file_name );

    device_t device( str
                   , typename detail::file_stream_device< FormatTag >::read_tag()
                   );

    delete[] str;

    return reader_backend< device_t, FormatTag >( device, settings );
}

#ifdef BOOST_GIL_IO_ADD_FS_PATH_SUPPORT
template< typename FormatTag >
inline
typename get_reader_backend< std::wstring
                           , FormatTag
                           >::type
make_reader_backend( const filesystem::path&                 path
                   , const image_read_settings< FormatTag >& settings
                   )
{
    return make_reader_backend( path.wstring()
                              , settings
                              );
}
#endif // BOOST_GIL_IO_ADD_FS_PATH_SUPPORT

template< typename Device
        , typename FormatTag
        >
inline
typename get_reader_backend< Device
                           , FormatTag
                           >::type
make_reader_backend( Device&                                 io_dev
                   , const image_read_settings< FormatTag >& settings
                   , typename enable_if< mpl::and_< detail::is_adaptable_input_device< FormatTag
                                                                                     , Device
                                                                                     >
                                                  , is_format_tag< FormatTag >
                                                  >
                                       >::type* /* ptr */ = 0
                   )
{
    typedef typename get_read_device< Device
                                    , FormatTag
                                    >::type device_t;

    device_t device( io_dev );

    return reader_backend< device_t, FormatTag >( device, settings );
}



template< typename String
        , typename FormatTag
        >
inline
typename get_reader_backend< String
                           , FormatTag
                           >::type
make_reader_backend( const String&    file_name
                   , const FormatTag&
                   , typename enable_if< mpl::and_< detail::is_supported_path_spec< String >
                                                  , is_format_tag< FormatTag >
                                                           >
                                       >::type* /* ptr */ = 0
                   )
{
    return make_reader_backend( file_name, image_read_settings< FormatTag >() );
}

template< typename Device
        , typename FormatTag
        >
inline
typename get_reader_backend< Device
                           , FormatTag
                           >::type
make_reader_backend( Device&          io_dev
                   , const FormatTag&
                   , typename enable_if< mpl::and_< detail::is_adaptable_input_device< FormatTag
                                                                                     , Device
                                                                                     >
                                                  , is_format_tag< FormatTag >
                                                  >
                                       >::type* /* ptr */ = 0
                   )
{
    return make_reader_backend( io_dev, image_read_settings< FormatTag >() );
}

} // namespace gil
} // namespace boost

#endif
