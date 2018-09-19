/*
    Copyright 2013 Christian Henning
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/********************************************************
 *
 * This test file will test rgb contig tiled tiff writing
 *
 *******************************************************/
//#define BOOST_TEST_MODULE tiff_tiled_rgb_contig_write_test_21_31_32_64_module
#include <boost/test/unit_test.hpp>

#include "tiff_tiled_write_macros.hpp"

BOOST_AUTO_TEST_SUITE( gil_io_tiff_tests )

#ifdef BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_PP_REPEAT_FROM_TO(21, 31, GENERATE_WRITE_TILE_BIT_ALIGNED_RGB, rgb )

BOOST_AUTO_TEST_CASE( write_tile_and_compare_with_rgb_strip_contig_32 )
{
    using namespace std;
    using namespace boost;
    using namespace gil;

    string filename_strip( tiff_in_GM + "tiger-rgb-strip-contig-32.tif" );

    typedef pixel< unsigned int, rgb_layout_t > rgb32_pixel_t;
    image< rgb32_pixel_t, false > img_strip, img_saved;

    read_image( filename_strip, img_strip, tag_t() );

    image_write_info<tag_t> info;
    info._is_tiled = true;
    info._tile_width = info._tile_length = 16;

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    write_view( tiff_out + "write_tile_and_compare_with_rgb_strip_contig_32.tif", view(img_strip), info );
    read_image( tiff_out + "write_tile_and_compare_with_rgb_strip_contig_32.tif", img_saved, tag_t() );

    BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_saved) ), true);
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

BOOST_AUTO_TEST_CASE( write_tile_and_compare_with_rgb_strip_contig_64 )
{
    using namespace std;
    using namespace boost;
    using namespace gil;

    string filename_strip( tiff_in_GM + "tiger-rgb-strip-contig-64.tif" );

    typedef pixel< uint64_t, rgb_layout_t > rgb64_pixel_t;
    image< rgb64_pixel_t, false > img_strip, img_saved;

    read_image( filename_strip, img_strip, tag_t() );

    image_write_info<tag_t> info;
    info._is_tiled = true;
    info._tile_width = info._tile_length = 16;

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    write_view( tiff_out + "write_tile_and_compare_with_rgb_strip_contig_64.tif", view(img_strip), info );
    read_image( tiff_out + "write_tile_and_compare_with_rgb_strip_contig_64.tif", img_saved, tag_t() );

    BOOST_CHECK_EQUAL( equal_pixels( const_view(img_strip), const_view(img_saved) ), true);
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

#endif // BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_AUTO_TEST_SUITE_END()
