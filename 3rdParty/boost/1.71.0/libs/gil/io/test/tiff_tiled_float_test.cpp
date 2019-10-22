//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
//#define BOOST_TEST_MODULE tiff_tiled_float_test_module

#include <boost/gil/extension/io/tiff.hpp>

#include <boost/test/unit_test.hpp>

#include "paths.hpp"

using namespace std;
using namespace boost;
using namespace gil;

using tag_t = tiff_tag;

BOOST_AUTO_TEST_SUITE( gil_io_tiff_tests )

#ifdef BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_AUTO_TEST_CASE( read_minisblack_float_tile_and_strip32 )
{
    std::string filename_strip( tiff_in_GM + "tiger-minisblack-float-strip-32.tif" );
    std::string filename_tile ( tiff_in_GM + "tiger-minisblack-float-tile-32.tif"  );

    gray32f_image_t img_strip, img_tile;

    read_image( filename_strip, img_strip, tag_t() );
    read_image( filename_tile,  img_tile,  tag_t() );

    BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_tile) ), true);
}

BOOST_AUTO_TEST_CASE( read_minisblack_float_tile_and_strip64 )
{
    std::string filename_strip( tiff_in_GM + "tiger-minisblack-float-strip-64.tif" );
    std::string filename_tile ( tiff_in_GM + "tiger-minisblack-float-tile-64.tif"  );

    gray64f_image_t img_strip, img_tile;

    read_image( filename_strip, img_strip, tag_t() );
    read_image( filename_tile,  img_tile,  tag_t() );

    BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_tile) ), true);
}

BOOST_AUTO_TEST_CASE( read_rgb_float_tile_and_strip_planar32 )
{
    std::string filename_strip( tiff_in_GM + "tiger-rgb-float-strip-planar-32.tif" );
    std::string filename_tile ( tiff_in_GM + "tiger-rgb-float-tile-planar-32.tif"  );

    rgb32f_image_t img_strip, img_tile;

    read_image( filename_strip, img_strip, tag_t() );
    read_image( filename_tile,  img_tile,  tag_t() );

    BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_tile) ), true);
}

BOOST_AUTO_TEST_CASE( read_rgb_float_tile_and_strip_contig32 )
{
    std::string filename_strip( tiff_in_GM + "tiger-rgb-float-strip-contig-32.tif" );
    std::string filename_tile ( tiff_in_GM + "tiger-rgb-float-tile-contig-32.tif"  );

    rgb32f_image_t img_strip, img_tile;

    read_image( filename_strip, img_strip, tag_t() );
    read_image( filename_tile,  img_tile,  tag_t() );

    BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_tile) ), true);
}

BOOST_AUTO_TEST_CASE( read_rgb_float_tile_and_strip64 )
{
    std::string filename_strip( tiff_in_GM + "tiger-rgb-float-strip-planar-64.tif" );
    std::string filename_tile ( tiff_in_GM + "tiger-rgb-float-tile-planar-64.tif"  );

    rgb64f_image_t img_strip, img_tile;

    read_image( filename_strip, img_strip, tag_t() );
    read_image( filename_tile,  img_tile,  tag_t() );

    BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_tile) ), true);
}

BOOST_AUTO_TEST_CASE( read_rgb_float_tile_and_strip_contig64 )
{
    std::string filename_strip( tiff_in_GM + "tiger-rgb-float-strip-contig-64.tif" );
    std::string filename_tile ( tiff_in_GM + "tiger-rgb-float-tile-contig-64.tif"  );

    rgb64f_image_t img_strip, img_tile;

    read_image( filename_strip, img_strip, tag_t() );
    read_image( filename_tile,  img_tile,  tag_t() );

    BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_tile) ), true);
}

#endif // BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_AUTO_TEST_SUITE_END()
