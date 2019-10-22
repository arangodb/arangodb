//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
//#define BOOST_TEST_MODULE tiff_tiled_miniblack_test_1_10_module

#include <boost/test/unit_test.hpp>

#include "tiff_tiled_read_macros.hpp"

BOOST_AUTO_TEST_SUITE( gil_io_tiff_tests )

#ifdef BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_PP_REPEAT_FROM_TO( 1,  8, GENERATE_TILE_STRIP_COMPARISON_BIT_ALIGNED_MINISBLACK, minisblack )
BOOST_PP_REPEAT_FROM_TO( 9, 11, GENERATE_TILE_STRIP_COMPARISON_BIT_ALIGNED_MINISBLACK, minisblack )

BOOST_AUTO_TEST_CASE( read_tile_and_compare_with_minisblack_strip_8 )
{
    using namespace std;
    using namespace boost;
    using namespace gil;

    string filename_strip( tiff_in_GM + "tiger-minisblack-strip-08.tif" );
    string filename_tile ( tiff_in_GM + "tiger-minisblack-tile-08.tif"  );

    gray8_image_t img_strip, img_tile;

    read_image( filename_strip, img_strip, tag_t() );
    read_image( filename_tile,  img_tile,  tag_t() );

    BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_tile) ), true);
}

#endif // BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_AUTO_TEST_SUITE_END()
