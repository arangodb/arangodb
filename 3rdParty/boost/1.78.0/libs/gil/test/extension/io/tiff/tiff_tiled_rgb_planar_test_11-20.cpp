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

BOOST_PP_REPEAT_FROM_TO(11, 16, GENERATE_TILE_STRIP_COMPARISON_BIT_ALIGNED_RGB, (rgb, planar))
BOOST_PP_REPEAT_FROM_TO(17, 21, GENERATE_TILE_STRIP_COMPARISON_BIT_ALIGNED_RGB, (rgb, planar))

void test_read_tile_and_compare_with_rgb_planar_strip_16()
{
    std::string filename_strip(tiff_in_GM + "tiger-rgb-strip-planar-16.tif");
    std::string filename_tile(tiff_in_GM + "tiger-rgb-tile-planar-16.tif");

    gil::rgb16_image_t img_strip, img_tile;

    gil::read_image(filename_strip, img_strip, gil::tiff_tag());
    gil::read_image(filename_tile, img_tile, gil::tiff_tag());

    BOOST_TEST(gil::equal_pixels(gil::const_view(img_strip), gil::const_view(img_tile)));
}

int main()
{
    test_read_tile_and_compare_with_rgb_planar_strip_16();

    // TODO: Make sure generated test cases are executed. See tiff_subimage_test.cpp. ~mloskot

    return boost::report_errors();
}

#else
int main() {}
#endif  // BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES
