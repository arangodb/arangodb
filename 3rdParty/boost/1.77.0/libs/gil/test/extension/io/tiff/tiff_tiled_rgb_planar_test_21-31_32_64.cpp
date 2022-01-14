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

#include <cstdint>
#include <string>

#include "tiff_tiled_read_macros.hpp"

namespace gil = boost::gil;

#ifdef BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

// FIXME: The (rgb, planar) construct does not seem to work! ~mloskot
BOOST_PP_REPEAT_FROM_TO(21, 32, GENERATE_TILE_STRIP_COMPARISON_BIT_ALIGNED_RGB, (rgb, planar))

void test_read_tile_and_compare_with_rgb_planar_strip_32()
{
    std::string filename_strip(tiff_in_GM + "tiger-rgb-strip-planar-32.tif");
    std::string filename_tile(tiff_in_GM + "tiger-rgb-tile-planar-32.tif");

    using rgb32_pixel_t = gil::pixel<unsigned int, gil::rgb_layout_t>;
    gil::image<rgb32_pixel_t, false> img_strip, img_tile;

    gil::read_image(filename_strip, img_strip, gil::tiff_tag());
    gil::read_image(filename_tile, img_tile, gil::tiff_tag());

    BOOST_TEST(gil::equal_pixels(gil::const_view(img_strip), gil::const_view(img_tile)));
}

void test_read_tile_and_compare_with_rgb_planar_strip_64()
{
    std::string filename_strip(tiff_in_GM + "tiger-rgb-strip-planar-64.tif");
    std::string filename_tile(tiff_in_GM + "tiger-rgb-tile-planar-64.tif");

    using rgb64_pixel_t = gil::pixel<std::uint64_t, gil::rgb_layout_t>;
    gil::image<rgb64_pixel_t, false> img_strip, img_tile;

    gil::read_image(filename_strip, img_strip, gil::tiff_tag());
    gil::read_image(filename_tile, img_tile, gil::tiff_tag());

    BOOST_TEST(gil::equal_pixels(gil::const_view(img_strip), gil::const_view(img_tile)));
}

int main()
{
    test_read_tile_and_compare_with_rgb_planar_strip_32();
    test_read_tile_and_compare_with_rgb_planar_strip_64();

    // TODO: Make sure generated test cases are executed. See tiff_subimage_test.cpp. ~mloskot

    return boost::report_errors();
}

#else
int main() {}
#endif  // BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES
