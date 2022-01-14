//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/io/tiff.hpp>

#include <boost/core/lightweight_test.hpp>

#include <string>

#include "tiff_tiled_read_macros.hpp"

namespace gil = boost::gil;

#ifdef BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

BOOST_PP_REPEAT_FROM_TO(1, 8, BOOST_GIL_TEST_GENERATE_TILE_STRIP_COMPARISON_BIT_ALIGNED_MINISBLACK, minisblack)
BOOST_PP_REPEAT_FROM_TO(9, 11, BOOST_GIL_TEST_GENERATE_TILE_STRIP_COMPARISON_BIT_ALIGNED_MINISBLACK, minisblack)

void test_read_tile_and_compare_with_minisblack_strip_8()
{
    std::string filename_strip(tiff_in_GM + "tiger-minisblack-strip-08.tif");
    std::string filename_tile(tiff_in_GM + "tiger-minisblack-tile-08.tif");

    gil::gray8_image_t img_strip, img_tile;
    gil::read_image(filename_strip, img_strip, gil::tiff_tag());
    gil::read_image(filename_tile, img_tile, gil::tiff_tag());

    BOOST_TEST(gil::equal_pixels(gil::const_view(img_strip), gil::const_view(img_tile)));
}

int main()
{
    test_read_tile_and_compare_with_minisblack_strip_8();

    // TODO: Make sure generated test cases are executed. See tiff_subimage_test.cpp. ~mloskot

    return boost::report_errors();
}

#else
int main() {}
#endif // BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES
