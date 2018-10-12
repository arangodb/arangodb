/*
    Copyright 2013 Christian Henning
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/********************************************************
 *
 * This test file will test RGB planar tiled tiff reading
 *
 *******************************************************/
//#define BOOST_TEST_MODULE tiff_tiled_rgb_planar_test_11_20_module
#include <boost/test/unit_test.hpp>

#include "tiff_tiled_read_macros.hpp"

BOOST_AUTO_TEST_SUITE( gil_io_tiff_tests )

#ifdef BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_PP_REPEAT_FROM_TO(11, 16, GENERATE_TILE_STRIP_COMPARISON_BIT_ALIGNED_RGB, (rgb,planar) )
BOOST_PP_REPEAT_FROM_TO(17, 21, GENERATE_TILE_STRIP_COMPARISON_BIT_ALIGNED_RGB, (rgb,planar) )

BOOST_AUTO_TEST_CASE( read_tile_and_compare_with_rgb_planar_strip_16 )
{
    using namespace std;
    using namespace boost;
    using namespace gil;

    string filename_strip( tiff_in_GM + "tiger-rgb-strip-planar-16.tif" );
    string filename_tile ( tiff_in_GM + "tiger-rgb-tile-planar-16.tif"  );

    rgb16_image_t img_strip, img_tile;

    read_image( filename_strip, img_strip, tag_t() );
    read_image( filename_tile,  img_tile,  tag_t() );

    BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_tile) ), true);
}

#endif // BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_AUTO_TEST_SUITE_END()
