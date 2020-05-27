//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
//#define BOOST_TEST_MODULE targa_old_test_module

#include <boost/gil.hpp>
#include <boost/gil/extension/io/targa/old.hpp>

#include <boost/test/unit_test.hpp>

#include "mandel_view.hpp"
#include "paths.hpp"

using namespace std;
using namespace boost;
using namespace gil;

BOOST_AUTO_TEST_SUITE( gil_io_targa_tests )

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_CASE( old_read_dimensions_test )
{
    boost::gil::point_t dim = targa_read_dimensions(targa_filename);
    BOOST_CHECK_EQUAL( dim.x, 124 );
    BOOST_CHECK_EQUAL( dim.y, 124 );
}

BOOST_AUTO_TEST_CASE( old_read_image_test )
{
    rgb8_image_t img;
    targa_read_image( targa_filename, img );

    BOOST_CHECK_EQUAL( img.width() , 124 );
    BOOST_CHECK_EQUAL( img.height(), 124 );
}

BOOST_AUTO_TEST_CASE( old_read_and_convert_image_test )
{
    rgb8_image_t img;
    targa_read_and_convert_image( targa_filename, img );

    BOOST_CHECK_EQUAL( img.width() , 124 );
    BOOST_CHECK_EQUAL( img.height(), 124 );
}

BOOST_AUTO_TEST_CASE( old_read_view_test )
{
    rgb8_image_t img( 124, 124 );
    targa_read_view( targa_filename, view( img ) );
}

BOOST_AUTO_TEST_CASE( old_read_and_convert_view_test )
{
    rgb8_image_t img( 124, 124 );
    targa_read_and_convert_view( targa_filename, view( img ) );
}

BOOST_AUTO_TEST_CASE( old_write_view_test )
{
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    targa_write_view( targa_out + "old_write_view_test.tga"
                    , create_mandel_view( 124, 124
                                        , rgb8_pixel_t( 0,   0, 255 )
                                        , rgb8_pixel_t( 0, 255,   0 )
                                        )
                    );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

BOOST_AUTO_TEST_CASE( old_dynamic_image_test )
{
    using my_img_types = mpl::vector
        <
            gray8_image_t,
            gray16_image_t,
            rgb8_image_t,
            rgba8_image_t
        >;

    any_image< my_img_types > runtime_image;

    targa_read_image( targa_filename.c_str()
                    , runtime_image
                    );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    targa_write_view( targa_out + "old_dynamic_image_test.tga"
                    , view( runtime_image )
                    );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_SUITE_END()
