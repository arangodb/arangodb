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

BOOST_PP_REPEAT_FROM_TO(21, 32, BOOST_GIL_TEST_GENERATE_TILE_STRIP_COMPARISON_BIT_ALIGNED_MINISBLACK, minisblack )

void test_read_tile_and_compare_with_minisblack_strip_32()
{
    std::string filename_strip(tiff_in_GM + "tiger-minisblack-strip-32.tif");
    std::string filename_tile(tiff_in_GM + "tiger-minisblack-tile-32.tif");

    using gray32_pixel_t = gil::pixel<unsigned int, gil::gray_layout_t>;
    gil::image<gray32_pixel_t, false> img_strip, img_tile;

    gil::read_image(filename_strip, img_strip, gil::tiff_tag());
    gil::read_image(filename_tile, img_tile, gil::tiff_tag());

    BOOST_TEST_EQ(gil::equal_pixels(gil::const_view(img_strip), gil::const_view(img_tile)), true);
}

void test_read_tile_and_compare_with_minisblack_strip_64()
{
    std::string filename_strip(tiff_in_GM + "tiger-minisblack-strip-64.tif");
    std::string filename_tile(tiff_in_GM + "tiger-minisblack-tile-64.tif");

    using gray64_pixel_t = gil::pixel<std::uint64_t, gil::gray_layout_t>;
    gil::image<gray64_pixel_t, false> img_strip, img_tile;

    gil::read_image(filename_strip, img_strip, gil::tiff_tag());
    gil::read_image(filename_tile, img_tile, gil::tiff_tag());

    BOOST_TEST_EQ(gil::equal_pixels(gil::const_view(img_strip), gil::const_view(img_tile)), true);
}

int main()
{
    test_read_tile_and_compare_with_minisblack_strip_32();
    test_read_tile_and_compare_with_minisblack_strip_64();

    // TODO: Make sure generated test cases are executed. See tiff_subimage_test.cpp. ~mloskot

    return boost::report_errors();
}

#else
int main() {}
#endif // BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES
