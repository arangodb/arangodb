//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
//#define BOOST_TEST_MODULE tiff_tiled_miniblack_test_21_31_32_64_module
#include <boost/test/unit_test.hpp>

#include <cstdint>
#include "tiff_tiled_read_macros.hpp"

BOOST_AUTO_TEST_SUITE( gil_io_tiff_tests )

#ifdef BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_PP_REPEAT_FROM_TO(21, 32, GENERATE_TILE_STRIP_COMPARISON_BIT_ALIGNED_MINISBLACK, minisblack )

BOOST_AUTO_TEST_CASE( read_tile_and_compare_with_minisblack_strip_32 )
{
    using namespace std;
    using namespace boost;
    using namespace gil;

    string filename_strip( tiff_in_GM + "tiger-minisblack-strip-32.tif" );
    string filename_tile ( tiff_in_GM + "tiger-minisblack-tile-32.tif"  );

    using gray32_pixel_t = pixel<unsigned int, gray_layout_t>;
    image< gray32_pixel_t, false > img_strip, img_tile;

    read_image( filename_strip, img_strip, tag_t() );
    read_image( filename_tile,  img_tile,  tag_t() );

    BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_tile) ), true);
}

BOOST_AUTO_TEST_CASE( read_tile_and_compare_with_minisblack_strip_64 )
{
    using namespace std;
    using namespace boost;
    using namespace gil;

    string filename_strip( tiff_in_GM + "tiger-minisblack-strip-64.tif" );
    string filename_tile ( tiff_in_GM + "tiger-minisblack-tile-64.tif"  );

    using gray64_pixel_t = pixel<uint64_t, gray_layout_t>;
    image< gray64_pixel_t, false > img_strip, img_tile;

    read_image( filename_strip, img_strip, tag_t() );
    read_image( filename_tile,  img_tile,  tag_t() );

    BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_tile) ), true);
}

#endif // BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_AUTO_TEST_SUITE_END()
