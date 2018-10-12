/*
    Copyright 2008 Christian Henning
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_EXTENSION_IO_BMP_OLD_HPP
#define BOOST_GIL_EXTENSION_IO_BMP_OLD_HPP

////////////////////////////////////////////////////////////////////////////////////////
/// \file
/// \brief
/// \author Christian Henning \n
///
/// \date 2008 \n
///
////////////////////////////////////////////////////////////////////////////////////////

#include <boost/gil/extension/io/bmp.hpp>

namespace boost { namespace gil {

/// \ingroup BMP_IO
/// \brief Returns the width and height of the BMP file at the specified location.
/// Throws std::ios_base::failure if the location does not correspond to a valid BMP file
template< typename String >
inline
point2< std::ptrdiff_t > bmp_read_dimensions( const String& filename )
{
    typedef typename get_reader_backend< String
                                       , bmp_tag
                                       >::type backend_t;

    backend_t backend = read_image_info( filename
                                       , bmp_tag()
                                       );

    return point2< std::ptrdiff_t >( backend._info._width
                                   , backend._info._height
                                   );
}


/// \ingroup BMP_IO
/// \brief Loads the image specified by the given bmp image file name into the given view.
/// Triggers a compile assert if the view color space and channel depth are not supported by the BMP library or by the I/O extension.
/// Throws std::ios_base::failure if the file is not a valid BMP file, or if its color space or channel depth are not
/// compatible with the ones specified by View, or if its dimensions don't match the ones of the view.
template< typename String
        , typename View
        >
inline
void bmp_read_view( const String& filename
                  , const View&   view
                  )
{
    read_view( filename
             , view
             , bmp_tag()
             );
}

/// \ingroup BMP_IO
/// \brief Allocates a new image whose dimensions are determined by the given bmp image file, and loads the pixels into it.
/// Triggers a compile assert if the image color space or channel depth are not supported by the BMP library or by the I/O extension.
/// Throws std::ios_base::failure if the file is not a valid BMP file, or if its color space or channel depth are not
/// compatible with the ones specified by Image
template< typename String
        , typename Image
        >
inline
void bmp_read_image( const String& filename
                   , Image&        img
                   )
{
    read_image( filename
              , img
              , bmp_tag()
              );
}

/// \ingroup BMP_IO
/// \brief Loads and color-converts the image specified by the given bmp image file name into the given view.
/// Throws std::ios_base::failure if the file is not a valid BMP file, or if its dimensions don't match the ones of the view.
template< typename String
        , typename View
        , typename CC
        >
inline
void bmp_read_and_convert_view( const String& filename
                              , const View&   view
                              , CC            cc
                              )
{
    read_and_convert_view( filename
                         , view
                         , cc
                         , bmp_tag()
                         );
}

/// \ingroup BMP_IO
/// \brief Loads and color-converts the image specified by the given bmp image file name into the given view.
/// Throws std::ios_base::failure if the file is not a valid BMP file, or if its dimensions don't match the ones of the view.
template< typename String
        , typename View
        >
inline
void bmp_read_and_convert_view( const String& filename
                              , const View&   view
                              )
{
    read_and_convert_view( filename
                         , view
                         , bmp_tag()
                         );
}

/// \ingroup BMP_IO
/// \brief Allocates a new image whose dimensions are determined by the given bmp image file, loads and color-converts the pixels into it.
/// Throws std::ios_base::failure if the file is not a valid BMP file
template< typename String
        , typename Image
        , typename CC
        >
inline
void bmp_read_and_convert_image( const String& filename
                               , Image&        img
                               , CC            cc
                               )
{
    read_and_convert_image( filename
                          , img
                          , cc
                          , bmp_tag()
                          );
}

/// \ingroup BMP_IO
/// \brief Allocates a new image whose dimensions are determined by the given bmp image file, loads and color-converts the pixels into it.
/// Throws std::ios_base::failure if the file is not a valid BMP file
template< typename String
        , typename Image
        >
inline
void bmp_read_and_convert_image( const String filename
                               , Image&       img
                               )
{
    read_and_convert_image( filename
                          , img
                          , bmp_tag()
                          );
}


/// \ingroup BMP_IO
/// \brief Saves the view to a bmp file specified by the given bmp image file name.
/// Triggers a compile assert if the view color space and channel depth are not supported by the BMP library or by the I/O extension.
/// Throws std::ios_base::failure if it fails to create the file.
template< typename String
        , typename View
        >
inline
void bmp_write_view( const String& filename
                   , const View&   view
                   )
{
    write_view( filename
              , view
              , bmp_tag()
              );
}

} // namespace gil
} // namespace boost

#endif
