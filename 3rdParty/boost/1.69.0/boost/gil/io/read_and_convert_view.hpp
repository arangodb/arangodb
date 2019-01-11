//
// Copyright 2007-2012 Christian Henning, Andreas Pokorny, Lubomir Bourdev
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_IO_READ_AND_CONVERT_VIEW_HPP
#define BOOST_GIL_IO_READ_AND_CONVERT_VIEW_HPP

#include <boost/gil/io/base.hpp>
#include <boost/gil/io/conversion_policies.hpp>
#include <boost/gil/io/device.hpp>
#include <boost/gil/io/get_reader.hpp>
#include <boost/gil/io/path_spec.hpp>

#include <boost/mpl/and.hpp>
#include <boost/type_traits/is_base_and_derived.hpp>
#include <boost/utility/enable_if.hpp>

namespace boost{ namespace gil {

/// \ingroup IO

/// \brief Reads and color-converts an image view. No memory is allocated.
/// \param reader    An image reader.
/// \param img       The image in which the data is read into.
/// \param settings  Specifies read settings depending on the image format.
/// \param cc        Color converter function object.
/// \throw std::ios_base::failure
template< typename Reader
        , typename View
        >
inline
void read_and_convert_view( Reader&     reader
                          , const View& view
                          , typename enable_if< mpl::and_< detail::is_reader< Reader >
                                                         , is_format_tag< typename Reader::format_tag_t >
                                                         >
                          >::type* /* ptr */ = 0
                          )
{
    reader.check_image_size( view.dimensions() );

    reader.init_view( view
                    , reader._settings
                    );

    reader.apply( view );
}

/// \brief Reads and color-converts an image view. No memory is allocated.
/// \param file      It's a device. Must satisfy is_input_device metafunction.
/// \param view      The image view in which the data is read into.
/// \param settings  Specifies read settings depending on the image format.
/// \param cc        Color converter function object.
/// \throw std::ios_base::failure
template< typename Device
        , typename View
        , typename ColorConverter
        , typename FormatTag
        >
inline
void read_and_convert_view( Device&                                 device
                          , const View&                             view
                          , const image_read_settings< FormatTag >& settings
                          , const ColorConverter&                   cc
                          , typename enable_if< mpl::and_< detail::is_read_device< FormatTag
                                                                                 , Device
                                                                                 >
                                                         , is_format_tag< FormatTag >
                                                         >
                                              >::type* /* ptr */ = 0

                          )
{
    typedef typename get_reader< Device
                               , FormatTag
                               , detail::read_and_convert< ColorConverter >
                               >::type reader_t;

    reader_t reader = make_reader( device
                                 , settings
                                 , detail::read_and_convert< ColorConverter >( cc )
                                 );

    read_and_convert_view( reader
                         , view
                         );
}

/// \brief Reads and color-converts an image view. No memory is allocated.
/// \param file_name File name. Must satisfy is_supported_path_spec metafunction.
/// \param view      The image view in which the data is read into.
/// \param settings  Specifies read settings depending on the image format.
/// \param cc        Color converter function object.
/// \throw std::ios_base::failure
template < typename String
         , typename View
         , typename ColorConverter
         , typename FormatTag
         >
inline
void read_and_convert_view( const String&                           file_name
                          , const View&                             view
                          , const image_read_settings< FormatTag >& settings
                          , const ColorConverter&                   cc
                          , typename enable_if< mpl::and_< is_format_tag< FormatTag >
                                                         , detail::is_supported_path_spec< String >
                                                         >
                                            >::type* /* ptr */ = 0
                          )
{
    typedef typename get_reader< String
                               , FormatTag
                               , detail::read_and_convert< ColorConverter >
                               >::type reader_t;

    reader_t reader = make_reader( file_name
                                 , settings
                                 , detail::read_and_convert< ColorConverter >( cc )
                                 );

    read_and_convert_view( reader
                         , view
                         );
}

/// \brief Reads and color-converts an image view. No memory is allocated.
/// \param file_name File name. Must satisfy is_supported_path_spec metafunction.
/// \param view      The image view in which the data is read into.
/// \param cc        Color converter function object.
/// \param tag       Defines the image format. Must satisfy is_format_tag metafunction.
/// \throw std::ios_base::failure
template < typename String
         , typename View
         , typename ColorConverter
         , typename FormatTag
         >
inline
void read_and_convert_view( const String&         file_name
                          , const View&           view
                          , const ColorConverter& cc
                          , const FormatTag&      tag
                          , typename enable_if< mpl::and_< is_format_tag< FormatTag >
                                                         , detail::is_supported_path_spec< String >
                                                         >
                                            >::type* /* ptr */ = 0
                          )
{
    typedef typename get_reader< String
                               , FormatTag
                               , detail::read_and_convert< ColorConverter >
                               >::type reader_t;

    reader_t reader = make_reader( file_name
                                 , tag
                                 , detail::read_and_convert< ColorConverter >( cc )
                                 );

    read_and_convert_view( reader
                         , view
                         );
}

/// \brief Reads and color-converts an image view. No memory is allocated.
/// \param file It's a device. Must satisfy is_input_device metafunction or is_adaptable_input_device.
/// \param view The image view in which the data is read into.
/// \param cc   Color converter function object.
/// \param tag  Defines the image format. Must satisfy is_format_tag metafunction.
/// \throw std::ios_base::failure
template < typename Device
         , typename View
         , typename ColorConverter
         , typename FormatTag
         >
inline
void read_and_convert_view( Device&               device
                          , const View&           view
                          , const ColorConverter& cc
                          , const FormatTag&      tag
                          , typename enable_if< mpl::and_< detail::is_read_device< FormatTag
                                                                                 , Device
                                                                                 >
                                                         , is_format_tag< FormatTag >
                                                         >
                                               >::type* /* ptr */ = 0
                          )
{
    typedef typename get_reader< Device
                               , FormatTag
                               , detail::read_and_convert< ColorConverter >
                               >::type reader_t;

    reader_t reader = make_reader( device
                                 , tag
                                 , detail::read_and_convert< ColorConverter >( cc )
                                 );

    read_and_convert_view( reader
                         , view
                         );
}

/// \brief Reads and color-converts an image view. No memory is allocated.
/// \param file_name File name. Must satisfy is_supported_path_spec metafunction.
/// \param view      The image view in which the data is read into.
/// \param settings  Specifies read settings depending on the image format.
/// \throw std::ios_base::failure
template < typename String
         , typename View
         , typename FormatTag
         >
inline
void read_and_convert_view( const String&                           file_name
                          , const View&                             view
                          , const image_read_settings< FormatTag >& settings
                          , typename enable_if< mpl::and_< is_format_tag< FormatTag >
                                                         , detail::is_supported_path_spec< String >
                                                         >
                                            >::type* /* ptr */ = 0
                          )
{
    typedef typename get_reader< String
                               , FormatTag
                               , detail::read_and_convert< default_color_converter >
                               >::type reader_t;

    reader_t reader = make_reader( file_name
                                 , settings
                                 , detail::read_and_convert< default_color_converter >()
                                 );

    read_and_convert_view( reader
                         , view
                         );
}

/// \brief Reads and color-converts an image view. No memory is allocated.
/// \param file      It's a device. Must satisfy is_input_device metafunction or is_adaptable_input_device.
/// \param view      The image view in which the data is read into.
/// \param settings  Specifies read settings depending on the image format.
/// \throw std::ios_base::failure
template < typename Device
         , typename View
         , typename FormatTag
         >
inline
void read_and_convert_view( Device&                                 device
                          , const View&                             view
                          , const image_read_settings< FormatTag >& settings
                          , typename enable_if< mpl::and_< detail::is_read_device< FormatTag
                                                                                 , Device
                                                                                 >
                                                         , is_format_tag< FormatTag >
                                                         >
                                               >::type* /* ptr */ = 0
                          )
{
    typedef typename get_reader< Device
                               , FormatTag
                               , detail::read_and_convert< default_color_converter >
                               >::type reader_t;

    reader_t reader = make_reader( device
                                 , settings
                                 , detail::read_and_convert< default_color_converter >()
                                 );

    read_and_convert_view( reader
                         , view
                         );

}

/// \brief Reads and color-converts an image view. No memory is allocated.
/// \param file_name File name. Must satisfy is_supported_path_spec metafunction.
/// \param view      The image view in which the data is read into.
/// \param tag       Defines the image format. Must satisfy is_format_tag metafunction.
/// \throw std::ios_base::failure
template < typename String
         , typename View
         , typename FormatTag
         >
inline
void read_and_convert_view( const String&    file_name
                          , const View&      view
                          , const FormatTag& tag
                          , typename enable_if< mpl::and_< is_format_tag< FormatTag >
                                                         , detail::is_supported_path_spec< String >
                                                         >
                                              >::type* /* ptr */ = 0
                          )
{
    typedef typename get_reader< String
                               , FormatTag
                               , detail::read_and_convert< default_color_converter >
                               >::type reader_t;

    reader_t reader = make_reader( file_name
                                 , tag
                                 , detail::read_and_convert< default_color_converter >()
                                 );

    read_and_convert_view( reader
                         , view
                         );
}

/// \brief Reads and color-converts an image view. No memory is allocated.
/// \param file It's a device. Must satisfy is_input_device metafunction or is_adaptable_input_device.
/// \param view The image view in which the data is read into.
/// \param tag  Defines the image format. Must satisfy is_format_tag metafunction.
/// \throw std::ios_base::failure
template < typename Device
         , typename View
         , typename FormatTag
         >
inline
void read_and_convert_view( Device&          device
                          , const View&      view
                          , const FormatTag& tag
                          , typename enable_if< mpl::and_< detail::is_read_device< FormatTag
                                                                                 , Device
                                                                                 >
                                                         , is_format_tag< FormatTag >
                                                         >
                                               >::type* /* ptr */ = 0
                          )
{
    typedef typename get_reader< Device
                               , FormatTag
                               , detail::read_and_convert< default_color_converter >
                               >::type reader_t;

    reader_t reader = make_reader( device
                                 , tag
                                 , detail::read_and_convert< default_color_converter >()
                                 );

    read_and_convert_view( reader
                         , view
                         );
}

} // namespace gil
} // namespace boost

#endif
