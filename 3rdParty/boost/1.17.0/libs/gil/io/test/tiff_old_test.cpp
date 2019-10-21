//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
//#define BOOST_TEST_MODULE tiff_old_test_module

#include <boost/gil.hpp>
#include <boost/gil/extension/io/tiff/old.hpp>

#include <boost/test/unit_test.hpp>

#include "paths.hpp"

// This test file will only test the old library's interface.
// It's more of a compile time test than a runtime test.

using namespace std;
using namespace boost;
using namespace gil;

BOOST_AUTO_TEST_SUITE( gil_io_tiff_tests )

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_CASE( old_read_dimensions_test )
{
    boost::gil::point_t dim = tiff_read_dimensions(tiff_filename);
    BOOST_CHECK_EQUAL( dim.x, 1000 );
    BOOST_CHECK_EQUAL( dim.y,  600 );
}

BOOST_AUTO_TEST_CASE( old_read_image_test )
{
    rgba8_image_t img;
    tiff_read_image( tiff_filename, img );

    BOOST_CHECK_EQUAL( img.width() , 1000 );
    BOOST_CHECK_EQUAL( img.height(),  600 );
}

BOOST_AUTO_TEST_CASE( old_read_and_convert_image_test )
{
    rgb8_image_t img;
    tiff_read_and_convert_image( tiff_filename, img );

    BOOST_CHECK_EQUAL( img.width() , 1000 );
    BOOST_CHECK_EQUAL( img.height(),  600 );
}

BOOST_AUTO_TEST_CASE( old_read_view_test )
{
    rgba8_image_t img( 1000, 600 );
    tiff_read_view( tiff_filename, view( img ) );
}

BOOST_AUTO_TEST_CASE( old_read_and_convert_view_test )
{
    rgb8_image_t img( 1000, 600 );
    tiff_read_and_convert_view( tiff_filename, view( img ) );
}

BOOST_AUTO_TEST_CASE( old_dynamic_image_test )
{
    using my_img_types = mpl::vector
        <
            gray8_image_t,
            gray16_image_t,
            rgba8_image_t,
            gray1_image_t
        >;

    any_image< my_img_types > runtime_image;

    tiff_read_image( tiff_filename.c_str()
                   , runtime_image
                   );

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    tiff_write_view( tiff_out + "old_dynamic_image_test.tif"
                   , view( runtime_image )
                   );
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

BOOST_AUTO_TEST_SUITE_END()
