/*
    Copyright 2013 Christian Henning
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

//#define BOOST_TEST_MODULE jpeg_old_test_module
#include <boost/test/unit_test.hpp>

#include <boost/gil.hpp>
#include <boost/gil/extension/io/jpeg/old.hpp>

#include "mandel_view.hpp"
#include "paths.hpp"

using namespace std;
using namespace boost;
using namespace gil;

BOOST_AUTO_TEST_SUITE( gil_io_jpeg_tests )

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_CASE( old_read_dimensions_test )
{
    point2< std::ptrdiff_t > dim = jpeg_read_dimensions( jpeg_filename );

    BOOST_CHECK_EQUAL( dim.x, 1000 );
    BOOST_CHECK_EQUAL( dim.y,  600 );
}

BOOST_AUTO_TEST_CASE( old_read_image_test )
{
    rgb8_image_t img;
    jpeg_read_image( jpeg_filename, img );

    BOOST_CHECK_EQUAL( img.width() , 1000 );
    BOOST_CHECK_EQUAL( img.height(),  600 );
}

BOOST_AUTO_TEST_CASE( old_read_and_convert_image_test )
{
    rgb8_image_t img;
    jpeg_read_and_convert_image( jpeg_filename, img );

    BOOST_CHECK_EQUAL( img.width() , 1000 );
    BOOST_CHECK_EQUAL( img.height(),  600 );
}

BOOST_AUTO_TEST_CASE( old_read_view_test )
{
    rgb8_image_t img( 1000, 600  );
    jpeg_read_view( jpeg_filename, view( img ) );
}

BOOST_AUTO_TEST_CASE( old_read_and_convert_view_test )
{
    rgb8_image_t img( 1000, 600 );
    jpeg_read_and_convert_view( jpeg_filename, view( img ) );
}

BOOST_AUTO_TEST_CASE( old_write_view_test )
{
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    jpeg_write_view( jpeg_out + "old_write_test.jpg"
                   , create_mandel_view( 320, 240
                                       , rgb8_pixel_t( 0,   0, 255 )
                                       , rgb8_pixel_t( 0, 255,   0 )
                                       )
                   );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

BOOST_AUTO_TEST_CASE( old_dynamic_image_test )
{
    typedef mpl::vector< gray8_image_t
                       , gray16_image_t
                       , rgb8_image_t
                       , rgba8_image_t
                       > my_img_types;


    any_image< my_img_types > runtime_image;

    jpeg_read_image( jpeg_filename.c_str()
                   , runtime_image
                   );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    jpeg_write_view( jpeg_out + "old_dynamic_image_test.jpg"
                   , view( runtime_image )
                   );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_SUITE_END()
