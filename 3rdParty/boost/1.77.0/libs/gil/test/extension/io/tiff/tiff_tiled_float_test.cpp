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

#include "paths.hpp"

namespace gil  = boost::gil;

#ifdef BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

void test_read_minisblack_float_tile_and_strip32()
{
    std::string filename_strip(tiff_in_GM + "tiger-minisblack-float-strip-32.tif");
    std::string filename_tile(tiff_in_GM + "tiger-minisblack-float-tile-32.tif");

    gil::gray32f_image_t img_strip, img_tile;
    gil::read_image(filename_strip, img_strip, gil::tiff_tag());
    gil::read_image(filename_tile, img_tile, gil::tiff_tag());

    BOOST_TEST(gil::equal_pixels(gil::const_view(img_strip), gil::const_view(img_tile)));
}

void test_read_minisblack_float_tile_and_strip64()
{
    std::string filename_strip(tiff_in_GM + "tiger-minisblack-float-strip-64.tif");
    std::string filename_tile(tiff_in_GM + "tiger-minisblack-float-tile-64.tif");

    // FIXME: boost/gil/extension/io/tiff/detail/is_allowed.hpp:229:49: error: ambiguous partial specializations of 'Format_Type
    //        discovered during https://github.com/boostorg/gil/issues/461
    //gil::gray64f_image_t img_strip, img_tile;
    //gil::read_image(filename_strip, img_strip, gil::tiff_tag());
    //gil::read_image(filename_tile, img_tile, gil::tiff_tag());
    //BOOST_TEST(gil::equal_pixels(gil::const_view(img_strip), gil::const_view(img_tile)));
}

void test_read_rgb_float_tile_and_strip_planar32()
{
    std::string filename_strip(tiff_in_GM + "tiger-rgb-float-strip-planar-32.tif");
    std::string filename_tile(tiff_in_GM + "tiger-rgb-float-tile-planar-32.tif");

    gil::rgb32f_image_t img_strip, img_tile;
    gil::read_image(filename_strip, img_strip, gil::tiff_tag());
    gil::read_image(filename_tile, img_tile, gil::tiff_tag());

    BOOST_TEST(gil::equal_pixels(gil::const_view(img_strip), gil::const_view(img_tile)));
}

void test_read_rgb_float_tile_and_strip_contig32()
{
    std::string filename_strip(tiff_in_GM + "tiger-rgb-float-strip-contig-32.tif");
    std::string filename_tile(tiff_in_GM + "tiger-rgb-float-tile-contig-32.tif");

    gil::rgb32f_image_t img_strip, img_tile;
    gil::read_image(filename_strip, img_strip, gil::tiff_tag());
    gil::read_image(filename_tile, img_tile, gil::tiff_tag());

    BOOST_TEST(gil::equal_pixels(gil::const_view(img_strip), gil::const_view(img_tile)));
}

void test_read_rgb_float_tile_and_strip_contig64()
{
    std::string filename_strip(tiff_in_GM + "tiger-rgb-float-strip-contig-64.tif");
    std::string filename_tile(tiff_in_GM + "tiger-rgb-float-tile-contig-64.tif");

    // FIXME: boost/gil/extension/io/tiff/detail/is_allowed.hpp:229:49: error: ambiguous partial specializations of 'Format_Type
    //        discovered during https://github.com/boostorg/gil/issues/461
    //gil::rgb64f_image_t img_strip, img_tile;
    //gil::read_image(filename_strip, img_strip, gil::tiff_tag());
    //gil::read_image(filename_tile, img_tile, gil::tiff_tag());
    //BOOST_TEST(gil::equal_pixels(gil::const_view(img_strip), gil::const_view(img_tile)));
}

void test_read_rgb_float_tile_and_strip64()
{
    std::string filename_strip(tiff_in_GM + "tiger-rgb-float-strip-planar-64.tif");
    std::string filename_tile(tiff_in_GM + "tiger-rgb-float-tile-planar-64.tif");

    // FIXME: boost/gil/extension/io/tiff/detail/is_allowed.hpp:229:49: error: ambiguous partial specializations of 'Format_Type
    //        discovered during https://github.com/boostorg/gil/issues/461
    //gil::rgb64f_image_t img_strip, img_tile;
    //gil::read_image(filename_strip, img_strip, gil::tiff_tag());
    //gil::read_image(filename_tile, img_tile, gil::tiff_tag());
    //BOOST_TEST(gil::equal_pixels(gil::const_view(img_strip), gil::const_view(img_tile)));
}

int main()
{
    test_read_minisblack_float_tile_and_strip32();
    test_read_minisblack_float_tile_and_strip64();
    test_read_rgb_float_tile_and_strip_planar32();
    test_read_rgb_float_tile_and_strip_contig32();
    test_read_rgb_float_tile_and_strip_contig64();
    test_read_rgb_float_tile_and_strip64();

    return boost::report_errors();
}

#else
int main() {}
#endif // BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES

