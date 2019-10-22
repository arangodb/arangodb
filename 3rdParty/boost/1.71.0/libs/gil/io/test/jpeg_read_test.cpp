//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
//#define BOOST_TEST_MODULE jpeg_read_test_module
#include <boost/gil.hpp>
#include <boost/gil/extension/io/jpeg.hpp>

#include <boost/test/unit_test.hpp>

#include "paths.hpp"
#include "scanline_read_test.hpp"

using namespace std;
using namespace boost::gil;

using tag_t = jpeg_tag;

BOOST_AUTO_TEST_SUITE( gil_io_jpeg_tests )

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_CASE( read_header_test )
{
    {
        using backend_t = get_reader_backend<std::string const, tag_t>::type;

        backend_t backend = read_image_info( jpeg_filename
                                           , tag_t()
                                           );

        BOOST_CHECK_EQUAL( backend._info._width         , 1000u );
        BOOST_CHECK_EQUAL( backend._info._height        ,  600u );

        BOOST_CHECK_EQUAL( backend._info._num_components, 3         );
        BOOST_CHECK_EQUAL( backend._info._color_space   , JCS_YCbCr );

        BOOST_CHECK_EQUAL( backend._info._data_precision, 8         );
    }
}

BOOST_AUTO_TEST_CASE( read_pixel_density_test )
{
    using backend_t = get_reader_backend<std::string const, tag_t>::type;

    backend_t backend = read_image_info( jpeg_in + "EddDawson/36dpi.jpg"
                                       , tag_t()
                                       );

    rgb8_image_t img;
    read_image( jpeg_in + "EddDawson/36dpi.jpg", img, jpeg_tag() );

    image_write_info< jpeg_tag > write_settings;
    write_settings.set_pixel_dimensions( backend._info._width
                                       , backend._info._height
                                       , backend._info._pixel_width_mm
                                       , backend._info._pixel_height_mm
                                       );

    stringstream in_memory( ios_base::in | ios_base::out | ios_base::binary );
    write_view( in_memory, view( img ), write_settings );

    using backend2_t = get_reader_backend<stringstream, tag_t>::type;

    backend2_t backend2 = read_image_info( in_memory
                                         , tag_t()
                                         );

    // Because of rounding the two results differ slightly.
    if(  std::abs( backend._info._pixel_width_mm  - backend2._info._pixel_width_mm  ) > 10.0
      || std::abs( backend._info._pixel_height_mm - backend2._info._pixel_height_mm ) > 10.0
      )
    {
        BOOST_CHECK_EQUAL( 0, 1 );
    }
}

BOOST_AUTO_TEST_CASE( read_reference_images_test )
{
    using image_t = rgb8_image_t;
    image_t img;

    read_image( jpeg_filename
                , img
                , tag_t()
                );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    write_view( jpeg_out + "rgb8_test.jpg"
               , view( img )
               , tag_t()
               );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

BOOST_AUTO_TEST_CASE( dct_method_read_test )
{
    using image_t = rgb8_image_t;
    image_t img;

    image_read_settings< jpeg_tag > settings;
    settings._dct_method = jpeg_dct_method::fast;

    read_image( jpeg_filename
                , img
                , settings
                );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    write_view( jpeg_out + "fast_dct_read_test.jpg"
               , view( img )
               , tag_t()
               );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

BOOST_AUTO_TEST_CASE( read_reference_images_image_iterator_test )
{
    test_scanline_reader< rgb8_image_t, jpeg_tag >( jpeg_filename.c_str() );
}

#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_SUITE_END()
