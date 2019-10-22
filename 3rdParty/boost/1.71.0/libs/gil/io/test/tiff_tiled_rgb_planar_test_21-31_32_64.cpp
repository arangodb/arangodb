//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
//#define BOOST_TEST_MODULE tiff_tiled_rgb_planar_test_21_31_32_64_module
#include <boost/test/unit_test.hpp>

#include "tiff_tiled_read_macros.hpp"

BOOST_AUTO_TEST_SUITE( gil_io_tiff_tests )

#ifdef BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_PP_REPEAT_FROM_TO(21, 32, GENERATE_TILE_STRIP_COMPARISON_BIT_ALIGNED_RGB, (rgb,planar) )

BOOST_AUTO_TEST_CASE( read_tile_and_compare_with_rgb_planar_strip_32 )
{
    using namespace std;
    using namespace boost;
    using namespace gil;

    string filename_strip( tiff_in_GM + "tiger-rgb-strip-planar-32.tif" );
    string filename_tile ( tiff_in_GM + "tiger-rgb-tile-planar-32.tif"  );

    using rgb32_pixel_t = pixel<unsigned int, rgb_layout_t>;
    image< rgb32_pixel_t, false > img_strip, img_tile;

    read_image( filename_strip, img_strip, tag_t() );
    read_image( filename_tile,  img_tile,  tag_t() );

    BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_tile) ), true);
}

BOOST_AUTO_TEST_CASE( read_tile_and_compare_with_rgb_planar_strip_64 )
{
    using namespace std;
    using namespace boost;
    using namespace gil;

    string filename_strip( tiff_in_GM + "tiger-rgb-strip-planar-64.tif" );
    string filename_tile ( tiff_in_GM + "tiger-rgb-tile-planar-64.tif"  );

    using rgb64_pixel_t = pixel<uint64_t, rgb_layout_t>;
    image< rgb64_pixel_t, false > img_strip, img_tile;

    read_image( filename_strip, img_strip, tag_t() );
    read_image( filename_tile,  img_tile,  tag_t() );

    BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_tile) ), true);
}

#endif // BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_AUTO_TEST_SUITE_END()
