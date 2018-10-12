/*
    Copyright 2013 Christian Henning
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/********************************************************
 *
 * This test file will test RGB contig tiled tiff reading
 *
 *******************************************************/
//#define BOOST_TEST_MODULE tiff_tiled_rgb_contig_test_21_31_32_64_module
#include <boost/test/unit_test.hpp>

#include "tiff_tiled_read_macros.hpp"

BOOST_AUTO_TEST_SUITE( gil_io_tiff_tests )

#ifdef BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_PP_REPEAT_FROM_TO(21, 32, GENERATE_TILE_STRIP_COMPARISON_BIT_ALIGNED_RGB, (rgb,contig) )

BOOST_AUTO_TEST_CASE( read_tile_and_compare_with_rgb_contig_strip_32 )
{
    using namespace std;
    using namespace boost;
    using namespace gil;

    string filename_strip( tiff_in_GM + "tiger-rgb-strip-contig-32.tif" );
    string filename_tile ( tiff_in_GM + "tiger-rgb-tile-contig-32.tif"  );

    typedef pixel< unsigned int, rgb_layout_t > rgb32_pixel_t;
    image< rgb32_pixel_t, false > img_strip, img_tile;

    read_image( filename_strip, img_strip, tag_t() );
    read_image( filename_tile,  img_tile,  tag_t() );

    BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_tile) ), true);
}

BOOST_AUTO_TEST_CASE( read_tile_and_compare_with_rgb_contig_strip_64 )
{
    using namespace std;
    using namespace boost;
    using namespace gil;

    string filename_strip( tiff_in_GM + "tiger-rgb-strip-contig-64.tif" );
    string filename_tile ( tiff_in_GM + "tiger-rgb-tile-contig-64.tif"  );

    typedef pixel< uint64_t, rgb_layout_t > rgb64_pixel_t;
    image< rgb64_pixel_t, false > img_strip, img_tile;

    read_image( filename_strip, img_strip, tag_t() );
    read_image( filename_tile,  img_tile,  tag_t() );

    BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_tile) ), true);
}

#endif // BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_AUTO_TEST_SUITE_END()
