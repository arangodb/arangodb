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

void test_read_info_tile_minisblack_float()
{
    using backend_t = gil::get_reader_backend<std::string const, gil::tiff_tag>::type;
    backend_t backend =
        gil::read_image_info(tiff_in_GM + "tiger-minisblack-float-tile-16.tif", gil::tiff_tag());

    BOOST_TEST_EQ(backend._info._tile_width, 16);
    BOOST_TEST_EQ(backend._info._tile_length, 16);
}

void test_read_info_tile_minisblack()
{
    using backend_t = gil::get_reader_backend<std::string const, gil::tiff_tag>::type;
    backend_t backend =
        gil::read_image_info(tiff_in_GM + "tiger-minisblack-tile-08.tif", gil::tiff_tag());

    BOOST_TEST_EQ(backend._info._tile_width, 16);
    BOOST_TEST_EQ(backend._info._tile_length, 16);
}

void test_read_info_tile_palette()
{
    using backend_t = gil::get_reader_backend<std::string const, gil::tiff_tag>::type;
    backend_t backend =
        gil::read_image_info(tiff_in_GM + "tiger-palette-tile-08.tif", gil::tiff_tag());

    BOOST_TEST_EQ(backend._info._tile_width, 16);
    BOOST_TEST_EQ(backend._info._tile_length, 16);
}

void test_read_info_tile_rgb()
{
    using backend_t = gil::get_reader_backend<std::string const, gil::tiff_tag>::type;
    backend_t backend =
        gil::read_image_info(tiff_in_GM + "tiger-rgb-tile-contig-08.tif", gil::tiff_tag());

    BOOST_TEST_EQ(backend._info._tile_width, 16);
    BOOST_TEST_EQ(backend._info._tile_length, 16);
}

void test_read_info_tile_planar()
{
    using backend_t = gil::get_reader_backend<std::string const, gil::tiff_tag>::type;
    backend_t backend =
        gil::read_image_info(tiff_in_GM + "tiger-rgb-tile-planar-08.tif", gil::tiff_tag());

    BOOST_TEST_EQ(backend._info._tile_width, 16);
    BOOST_TEST_EQ(backend._info._tile_length, 16);
}

int main()
{
    test_read_info_tile_minisblack_float();
    test_read_info_tile_minisblack();
    test_read_info_tile_palette();
    test_read_info_tile_rgb();
    test_read_info_tile_planar();

    return boost::report_errors();
}

#else
int main() {}
#endif  // BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES
