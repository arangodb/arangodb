//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#define BOOST_GIL_IO_ADD_FS_PATH_SUPPORT
#define BOOST_GIL_IO_ENABLE_GRAY_ALPHA
#define BOOST_FILESYSTEM_VERSION 3
#include <boost/gil.hpp>
#include <boost/gil/extension/io/png.hpp>

#include <boost/core/lightweight_test.hpp>

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

#include "paths.hpp"
#include "scanline_read_test.hpp"
#include "test_utility_output_stream.hpp"

namespace gil = boost::gil;
namespace fs = boost::filesystem;

using gray_alpha8_pixel_t = gil::pixel<std::uint8_t, gil::gray_alpha_layout_t>;
using gray_alpha8c_pixel_t = gil::pixel<std::uint8_t, gil::gray_alpha_layout_t> const;
using gray_alpha8_image_t= gil::image<gray_alpha8_pixel_t, false>;

using gray_alpha16_pixel_t = gil::pixel<std::uint16_t, gil::gray_alpha_layout_t>;
using gray_alpha16c_pixel_t = gil::pixel<std::uint16_t, gil::gray_alpha_layout_t> const;
using gray_alpha16_image_t = gil::image<gray_alpha16_pixel_t, false>;

template <typename Image>
void test_file(std::string filename)
{
    Image src, dst;

    gil::image_read_settings<gil::png_tag> settings;
    settings._read_file_gamma        = true;
    settings._read_transparency_data = true;

    using backend_t   = gil::get_reader_backend<std::string const, gil::png_tag>::type;
    backend_t backend = gil::read_image_info(png_in + filename, settings);

    gil::read_image(png_in + filename, src, settings);

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::image_write_info<gil::png_tag> write_info;
    write_info._file_gamma = backend._info._file_gamma;

    gil::write_view(png_out + filename, view(src), write_info);
    gil::read_image(png_out + filename, dst, settings);

    BOOST_TEST(gil::equal_pixels(gil::const_view(src), gil::const_view(dst)));
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

template <typename Image>
void test_png_scanline_reader(std::string filename)
{
    test_scanline_reader<Image, gil::png_tag>(std::string(png_in + filename).c_str());
}

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
void test_read_header()
{
    using backend_t = gil::get_reader_backend<std::string const, gil::png_tag>::type;
    backend_t backend = gil::read_image_info(png_filename, gil::png_tag());

    BOOST_TEST_EQ(backend._info._width, 1000u);
    BOOST_TEST_EQ(backend._info._height, 600u);

    BOOST_TEST_EQ(backend._info._num_channels, 4);
    BOOST_TEST_EQ(backend._info._bit_depth, 8);
    BOOST_TEST_EQ(backend._info._color_type, PNG_COLOR_TYPE_RGBA);

    BOOST_TEST_EQ(backend._info._interlace_method, PNG_INTERLACE_NONE);
    BOOST_TEST_EQ(backend._info._compression_method, PNG_COMPRESSION_TYPE_BASE);
    BOOST_TEST_EQ(backend._info._filter_method, PNG_FILTER_TYPE_BASE);

    BOOST_TEST_EQ(backend._info._file_gamma, 1);
}

void test_read_pixel_per_meter()
{
    gil::image_read_settings<gil::png_tag> settings;
    settings.set_read_members_true();

    using backend_t = gil::get_reader_backend<std::string const, gil::png_tag>::type;
    backend_t backend = gil::read_image_info(png_base_in + "EddDawson/36dpi.png", settings);

    BOOST_TEST_EQ(backend._info._pixels_per_meter, png_uint_32(1417));
}

void test_read_with_trns_chunk_color_type_0()
{
    // PNG 1.2: For color type 0 (grayscale), the tRNS chunk contains a single gray level value,
    // stored in the format:
    //
    // Gray:  2 bytes, range 0 .. (2^bitdepth)-1
    {
        auto const png_path = png_in + "tbbn0g04.png";
        gil::image_read_settings<gil::png_tag> settings;
        settings.set_read_members_true();
        auto backend = gil::read_image_info(png_path, settings);
        BOOST_TEST_EQ(backend._info._width, 32u);
        BOOST_TEST_EQ(backend._info._height, 32u);
        BOOST_TEST_EQ(backend._info._bit_depth, 4);
        BOOST_TEST_EQ(backend._info._num_channels, 1u);
        BOOST_TEST_EQ(backend._info._color_type, PNG_COLOR_TYPE_GRAY);

        gray_alpha8_image_t img;
        read_image(png_path, img, settings);
        BOOST_TEST_EQ(backend._info._width, 32u);
        BOOST_TEST_EQ(backend._info._height, 32u);
        BOOST_TEST_EQ(const_view(img).front(), gray_alpha8c_pixel_t(255, 0));
        BOOST_TEST_EQ(const_view(img)[78], gray_alpha8c_pixel_t(221, 255));
        BOOST_TEST_EQ(const_view(img)[79], gray_alpha8c_pixel_t(204, 255));
        BOOST_TEST_EQ(const_view(img)[975], gray_alpha8c_pixel_t(238, 255));
        BOOST_TEST_EQ(const_view(img)[976], gray_alpha8c_pixel_t(221, 255));
        BOOST_TEST_EQ(const_view(img).back(), gray_alpha8c_pixel_t(255, 0));
    }
    {
        auto const png_path = png_in + "tbwn0g16.png";
        gil::image_read_settings<gil::png_tag> settings;
        settings.set_read_members_true();
        auto backend = gil::read_image_info(png_path, settings);
        BOOST_TEST_EQ(backend._info._width, 32u);
        BOOST_TEST_EQ(backend._info._height, 32u);
        BOOST_TEST_EQ(backend._info._bit_depth, 16);
        BOOST_TEST_EQ(backend._info._num_channels, 1u);
        BOOST_TEST_EQ(backend._info._color_type, PNG_COLOR_TYPE_GRAY);

        gray_alpha16_image_t img;
        read_image(png_path, img, settings);
        BOOST_TEST_EQ(backend._info._width, 32u);
        BOOST_TEST_EQ(backend._info._height, 32u);
        BOOST_TEST_EQ(const_view(img).front(), gray_alpha16c_pixel_t(65535, 0));
        BOOST_TEST_EQ(const_view(img)[78], gray_alpha16c_pixel_t(58339, 65535));
        BOOST_TEST_EQ(const_view(img)[79], gray_alpha16c_pixel_t(51657, 65535));
        BOOST_TEST_EQ(const_view(img)[975], gray_alpha16c_pixel_t(62965, 65535));
        BOOST_TEST_EQ(const_view(img)[976], gray_alpha16c_pixel_t(58339, 65535));
        BOOST_TEST_EQ(const_view(img).back(), gray_alpha16c_pixel_t(65535, 0));
    }
}

void test_read_with_trns_chunk_color_type_2()
{
    // PNG 1.2: For color type 2 (truecolor), the tRNS chunk contains a single RGB color value,
    // stored in the format:
    //
    // Red:   2 bytes, range 0 .. (2^bitdepth)-1
    // Green: 2 bytes, range 0 .. (2^bitdepth)-1
    // Blue:  2 bytes, range 0 .. (2^bitdepth)-1
    {
        auto const png_path = png_in + "tbbn2c16.png";
        gil::image_read_settings<gil::png_tag> settings;
        settings.set_read_members_true();
        auto backend = gil::read_image_info(png_path, settings);
        BOOST_TEST_EQ(backend._info._width, 32u);
        BOOST_TEST_EQ(backend._info._height, 32u);
        BOOST_TEST_EQ(backend._info._bit_depth, 16);
        BOOST_TEST_EQ(backend._info._num_channels, 3u);
        BOOST_TEST_EQ(backend._info._color_type, PNG_COLOR_TYPE_RGB);

        gil::rgba16_image_t img;
        read_image(png_path, img, settings);
        BOOST_TEST_EQ(backend._info._width, 32u);
        BOOST_TEST_EQ(backend._info._height, 32u);
        BOOST_TEST_EQ(const_view(img).front(), gil::rgba16c_pixel_t(65535, 65535, 65535, 0));
        BOOST_TEST_EQ(const_view(img)[78], gil::rgba16c_pixel_t(58339, 58339, 58339, 65535));
        BOOST_TEST_EQ(const_view(img)[79], gil::rgba16c_pixel_t(51657, 51657, 51657, 65535));
        BOOST_TEST_EQ(const_view(img)[975], gil::rgba16c_pixel_t(62965, 62965, 62965, 65535));
        BOOST_TEST_EQ(const_view(img)[976], gil::rgba16c_pixel_t(58339, 58339, 58339, 65535));
        BOOST_TEST_EQ(const_view(img).back(), gil::rgba16c_pixel_t(65535, 65535, 65535, 0));
    }
    {
        auto const png_path = png_in + "tbgn2c16.png";
        gil::image_read_settings<gil::png_tag> settings;
        settings.set_read_members_true();
        auto backend = gil::read_image_info(png_path, settings);
        BOOST_TEST_EQ(backend._info._width, 32u);
        BOOST_TEST_EQ(backend._info._height, 32u);
        BOOST_TEST_EQ(backend._info._bit_depth, 16);
        BOOST_TEST_EQ(backend._info._num_channels, 3u);
        BOOST_TEST_EQ(backend._info._color_type, PNG_COLOR_TYPE_RGB);

        gil::rgba16_image_t img;
        read_image(png_path, img, settings);
        BOOST_TEST_EQ(backend._info._width, 32u);
        BOOST_TEST_EQ(backend._info._height, 32u);
        BOOST_TEST_EQ(const_view(img).front(), gil::rgba16c_pixel_t(65535, 65535, 65535, 0));
        BOOST_TEST_EQ(const_view(img)[78], gil::rgba16c_pixel_t(58339, 58339, 58339, 65535));
        BOOST_TEST_EQ(const_view(img)[79], gil::rgba16c_pixel_t(51657, 51657, 51657, 65535));
        BOOST_TEST_EQ(const_view(img)[975], gil::rgba16c_pixel_t(62965, 62965, 62965, 65535));
        BOOST_TEST_EQ(const_view(img)[976], gil::rgba16c_pixel_t(58339, 58339, 58339, 65535));
        BOOST_TEST_EQ(const_view(img).back(), gil::rgba16c_pixel_t(65535, 65535, 65535, 0));
    }
    {
        auto const png_path = png_in + "tbrn2c08.png";
        gil::image_read_settings<gil::png_tag> settings;
        settings.set_read_members_true();
        auto backend = gil::read_image_info(png_path, settings);
        BOOST_TEST_EQ(backend._info._width, 32u);
        BOOST_TEST_EQ(backend._info._height, 32u);
        BOOST_TEST_EQ(backend._info._bit_depth, 8);
        BOOST_TEST_EQ(backend._info._num_channels, 3u);
        BOOST_TEST_EQ(backend._info._color_type, PNG_COLOR_TYPE_RGB);

        gil::rgba8_image_t img;
        read_image(png_path, img, settings);
        BOOST_TEST_EQ(backend._info._width, 32u);
        BOOST_TEST_EQ(backend._info._height, 32u);
        BOOST_TEST_EQ(const_view(img).front(), gil::rgba8c_pixel_t(255, 255, 255, 0));
        BOOST_TEST_EQ(const_view(img)[78], gil::rgba8c_pixel_t(227, 227, 227, 255));
        BOOST_TEST_EQ(const_view(img)[79], gil::rgba8c_pixel_t(201, 201, 201, 255));
        BOOST_TEST_EQ(const_view(img)[975], gil::rgba8c_pixel_t(245, 245, 245, 255));
        BOOST_TEST_EQ(const_view(img)[976], gil::rgba8c_pixel_t(227, 227, 227, 255));
        BOOST_TEST_EQ(const_view(img).back(), gil::rgba8c_pixel_t(255, 255, 255, 0));
    }
}

void test_read_with_trns_chunk_color_type_3()
{
    // PNG 1.2: For color type 3 (indexed color), the tRNS chunk contains a series of one-byte
    // alpha values, corresponding to entries in the PLTE chunk:
    //
    // Alpha for palette index 0:  1 byte
    // Alpha for palette index 1:  1 byte
    // ...etc...
    {
        auto const png_path = png_in + "tbbn3p08.png";
        gil::image_read_settings<gil::png_tag> settings;
        settings.set_read_members_true();
        auto backend = gil::read_image_info(png_path, settings);
        BOOST_TEST_EQ(backend._info._width, 32u);
        BOOST_TEST_EQ(backend._info._height, 32u);
        BOOST_TEST_EQ(backend._info._bit_depth, 8);
        BOOST_TEST_EQ(backend._info._num_channels, 1u);
        BOOST_TEST_EQ(backend._info._color_type, PNG_COLOR_TYPE_PALETTE);

        gil::rgba8_image_t img;
        read_image(png_path, img, settings);
        BOOST_TEST_EQ(backend._info._width, 32u);
        BOOST_TEST_EQ(backend._info._height, 32u);
        BOOST_TEST_EQ(const_view(img).front(), gil::rgba8c_pixel_t(255, 255, 255, 0));
        BOOST_TEST_EQ(const_view(img)[78], gil::rgba8c_pixel_t(227, 227, 227, 255));
        BOOST_TEST_EQ(const_view(img)[79], gil::rgba8c_pixel_t(201, 201, 201, 255));
        BOOST_TEST_EQ(const_view(img)[975], gil::rgba8c_pixel_t(246, 246, 246, 255));
        BOOST_TEST_EQ(const_view(img)[976], gil::rgba8c_pixel_t(227, 227, 227, 255));
        BOOST_TEST_EQ(const_view(img).back(), gil::rgba8c_pixel_t(255, 255, 255, 0));
    }
    {
        auto const png_path = png_in + "tbgn3p08.png";
        gil::image_read_settings<gil::png_tag> settings;
        settings.set_read_members_true();
        auto backend = gil::read_image_info(png_path, settings);
        BOOST_TEST_EQ(backend._info._width, 32u);
        BOOST_TEST_EQ(backend._info._height, 32u);
        BOOST_TEST_EQ(backend._info._bit_depth, 8);
        BOOST_TEST_EQ(backend._info._num_channels, 1u);
        BOOST_TEST_EQ(backend._info._color_type, PNG_COLOR_TYPE_PALETTE);

        gil::rgba8_image_t img;
        read_image(png_path, img, settings);
        BOOST_TEST_EQ(backend._info._width, 32u);
        BOOST_TEST_EQ(backend._info._height, 32u);
        BOOST_TEST_EQ(const_view(img).front(), gil::rgba8c_pixel_t(255, 255, 255, 0));
        BOOST_TEST_EQ(const_view(img)[78], gil::rgba8c_pixel_t(227, 227, 227, 255));
        BOOST_TEST_EQ(const_view(img)[79], gil::rgba8c_pixel_t(201, 201, 201, 255));
        BOOST_TEST_EQ(const_view(img)[975], gil::rgba8c_pixel_t(246, 246, 246, 255));
        BOOST_TEST_EQ(const_view(img)[976], gil::rgba8c_pixel_t(227, 227, 227, 255));
        BOOST_TEST_EQ(const_view(img).back(), gil::rgba8c_pixel_t(255, 255, 255, 0));
    }
    {
        auto const png_path = png_in + "tp1n3p08.png";
        gil::image_read_settings<gil::png_tag> settings;
        settings.set_read_members_true();
        auto backend = gil::read_image_info(png_path, settings);
        BOOST_TEST_EQ(backend._info._width, 32u);
        BOOST_TEST_EQ(backend._info._height, 32u);
        BOOST_TEST_EQ(backend._info._bit_depth, 8);
        BOOST_TEST_EQ(backend._info._num_channels, 1u);
        BOOST_TEST_EQ(backend._info._color_type, PNG_COLOR_TYPE_PALETTE);

        gil::rgba8_image_t img;
        read_image(png_path, img, settings);
        BOOST_TEST_EQ(backend._info._width, 32u);
        BOOST_TEST_EQ(backend._info._height, 32u);
        BOOST_TEST_EQ(const_view(img).front(), gil::rgba8c_pixel_t(255, 255, 255, 0));
        BOOST_TEST_EQ(const_view(img)[78], gil::rgba8c_pixel_t(227, 227, 227, 255));
        BOOST_TEST_EQ(const_view(img)[79], gil::rgba8c_pixel_t(201, 201, 201, 255));
        BOOST_TEST_EQ(const_view(img)[975], gil::rgba8c_pixel_t(246, 246, 246, 255));
        BOOST_TEST_EQ(const_view(img)[976], gil::rgba8c_pixel_t(227, 227, 227, 255));
        BOOST_TEST_EQ(const_view(img).back(), gil::rgba8c_pixel_t(255, 255, 255, 0));
    }
    {
        auto const png_path = png_in + "tm3n3p02.png";
        gil::image_read_settings<gil::png_tag> settings;
        settings.set_read_members_true();
        auto backend = gil::read_image_info(png_path, settings);
        BOOST_TEST_EQ(backend._info._width, 32u);
        BOOST_TEST_EQ(backend._info._height, 32u);
        BOOST_TEST_EQ(backend._info._bit_depth, 2);
        BOOST_TEST_EQ(backend._info._num_channels, 1u);
        BOOST_TEST_EQ(backend._info._color_type, PNG_COLOR_TYPE_PALETTE);

        gil::rgba8_image_t img;
        read_image(png_path, img, settings);
        BOOST_TEST_EQ(backend._info._width, 32u);
        BOOST_TEST_EQ(backend._info._height, 32u);
        BOOST_TEST_EQ(const_view(img).front(), gil::rgba8c_pixel_t(0, 0, 255, 0));
        BOOST_TEST_EQ(const_view(img)[16], gil::rgba8c_pixel_t(0, 0, 255, 85));
        BOOST_TEST_EQ(const_view(img)[511], gil::rgba8c_pixel_t(0, 0, 255, 85));
        BOOST_TEST_EQ(const_view(img)[1007], gil::rgba8c_pixel_t(0, 0, 255, 170));
        BOOST_TEST_EQ(const_view(img).back(), gil::rgba8c_pixel_t(0, 0, 255, 255));
    }
}
#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

#ifdef BOOST_GIL_IO_USE_PNG_TEST_SUITE_IMAGES
void test_basic_format()
{
    // Basic format test files (non-interlaced)

    // BASN0g01    -   black & white
    test_file<gil::gray1_image_t>("BASN0G01.PNG");
    test_png_scanline_reader<gil::gray1_image_t>("BASN0G01.PNG");

    // BASN0g02    -   2 bit (4 level) grayscale
    test_file<gil::gray2_image_t>("BASN0G02.PNG");
    test_png_scanline_reader<gil::gray2_image_t>("BASN0G02.PNG");

    // BASN0g04    -   4 bit (16 level) grayscale
    test_file<gil::gray4_image_t>("BASN0G04.PNG");
    test_png_scanline_reader<gil::gray4_image_t>("BASN0G04.PNG");

    // BASN0g08    -   8 bit (256 level) grayscale
    test_file<gil::gray8_image_t>("BASN0G08.PNG");
    test_png_scanline_reader<gil::gray8_image_t>("BASN0G08.PNG");

    // BASN0g16    -   16 bit (64k level) grayscale
    test_file<gil::gray16_image_t>("BASN0G16.PNG");
    test_png_scanline_reader<gil::gray16_image_t>("BASN0G16.PNG");

    // BASN2c08    -   3x8 bits gil::rgb color
    test_file<gil::rgb8_image_t>("BASN2C08.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("BASN2C08.PNG");

    // BASN2c16    -   3x16 bits gil::rgb color
    test_file<gil::rgb16_image_t>("BASN2C16.PNG");
    test_png_scanline_reader<gil::rgb16_image_t>("BASN2C16.PNG");

    // BASN3p01    -   1 bit (2 color) paletted
    test_file<gil::rgb8_image_t>("BASN3P01.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("BASN3P01.PNG");

    // BASN3p02    -   2 bit (4 color) paletted
    test_file<gil::rgb8_image_t>("BASN3P02.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("BASN3P02.PNG");

    // BASN3p04    -   4 bit (16 color) paletted
    test_file<gil::rgb8_image_t>("BASN3P04.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("BASN3P04.PNG");

    // BASN3p08    -   8 bit (256 color) paletted
    test_file<gil::rgb8_image_t>("BASN3P08.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("BASN3P08.PNG");

    // BASN4a08    -   8 bit grayscale + 8 bit alpha-channel
    test_file<gil::gray_alpha8_image_t>("BASN4A08.PNG");
    test_png_scanline_reader<gil::gray_alpha8_image_t>("BASN4A08.PNG");

    // BASN4a16    -   16 bit grayscale + 16 bit alpha-channel
    test_file<gil::gray_alpha16_image_t>("BASN4A16.PNG");
    test_png_scanline_reader<gil::gray_alpha16_image_t>("BASN4A16.PNG");

    // BASN6a08    -   3x8 bits gil::rgb color + 8 bit alpha-channel
    test_file<gil::rgba8_image_t>("BASN6A08.PNG");
    test_png_scanline_reader<gil::rgba8_image_t>("BASN6A08.PNG");

    // BASN6a16    -   3x16 bits gil::rgb color + 16 bit alpha-channel
    test_file<gil::rgba16_image_t>("BASN6A16.PNG");
    test_png_scanline_reader<gil::rgba16_image_t>("BASN6A16.PNG");
}

void test_basic_format_interlaced()
{
    // Basic format test files (Adam-7 interlaced)

    // BASI0g01    -   black & white
    test_file<gil::gray1_image_t>("BASI0G01.PNG");

    // BASI0g02    -   2 bit (4 level) grayscale
    test_file<gil::gray2_image_t>("BASI0G02.PNG");

    // BASI0g04    -   4 bit (16 level) grayscale
    test_file<gil::gray4_image_t>("BASI0G04.PNG");

    // BASI0g08    -   8 bit (256 level) grayscale
    test_file<gil::gray8_image_t>("BASI0G08.PNG");

    // BASI0g16    -   16 bit (64k level) grayscale
    test_file<gil::gray16_image_t>("BASI0G16.PNG");

    // BASI2c08    -   3x8 bits gil::rgb color
    test_file<gil::rgb8_image_t>("BASI2C08.PNG");

    // BASI2c16    -   3x16 bits gil::rgb color
    test_file<gil::rgb16_image_t>("BASI2C16.PNG");

    // BASI3p01    -   1 bit (2 color) paletted
    test_file<gil::rgb8_image_t>("BASI3P01.PNG");

    // BASI3p02    -   2 bit (4 color) paletted
    test_file<gil::rgb8_image_t>("BASI3P02.PNG");

    // BASI3p04    -   4 bit (16 color) paletted
    test_file<gil::rgb8_image_t>("BASI3P04.PNG");

    // BASI3p08    -   8 bit (256 color) paletted
    test_file<gil::rgb8_image_t>("BASI3P08.PNG");

    // BASI4a08    -   8 bit grayscale + 8 bit alpha-channel
    test_file<gil::gray_alpha8_image_t>("BASI4A08.PNG");

    // BASI4a16    -   16 bit grayscale + 16 bit alpha-channel
    test_file<gil::gray_alpha16_image_t>("BASI4A16.PNG");

    // BASI6a08    -   3x8 bits gil::rgb color + 8 bit alpha-channel
    test_file<gil::rgba8_image_t>("BASI6A08.PNG");

    // BASI6a16    -   3x16 bits gil::rgb color + 16 bit alpha-channel
    test_file<gil::rgba16_image_t>("BASI6A16.PNG");
}

void test_odd_sizes()
{
    // S01I3P01 - 1x1 paletted file, interlaced
    test_file<gil::rgb8_image_t>("S01I3P01.PNG");

    // S01N3P01 - 1x1 paletted file, no interlacing
    test_file<gil::rgb8_image_t>("S01N3P01.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("S01N3P01.PNG");

    // S02I3P01 - 2x2 paletted file, interlaced
    test_file<gil::rgb8_image_t>("S02I3P01.PNG");

    // S02N3P01 - 2x2 paletted file, no interlacing
    test_file<gil::rgb8_image_t>("S02N3P01.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("S02N3P01.PNG");

    // S03I3P01 - 3x3 paletted file, interlaced
    test_file<gil::rgb8_image_t>("S03I3P01.PNG");

    // S03N3P01 - 3x3 paletted file, no interlacing
    test_file<gil::rgb8_image_t>("S03N3P01.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("S03N3P01.PNG");

    // S04I3P01 - 4x4 paletted file, interlaced
    test_file<gil::rgb8_image_t>("S04I3P01.PNG");

    // S04N3P01 - 4x4 paletted file, no interlacing
    test_file<gil::rgb8_image_t>("S04N3P01.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("S04N3P01.PNG");

    // S05I3P02 - 5x5 paletted file, interlaced
    test_file<gil::rgb8_image_t>("S05I3P02.PNG");

    // S05N3P02 - 5x5 paletted file, no interlacing
    test_file<gil::rgb8_image_t>("S05N3P02.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("S05N3P02.PNG");

    // S06I3P02 - 6x6 paletted file, interlaced
    test_file<gil::rgb8_image_t>("S06I3P02.PNG");

    // S06N3P02 - 6x6 paletted file, no interlacing
    test_file<gil::rgb8_image_t>("S06N3P02.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("S06N3P02.PNG");

    // S07I3P02 - 7x7 paletted file, interlaced
    test_file<gil::rgb8_image_t>("S07I3P02.PNG");

    // S07N3P02 - 7x7 paletted file, no interlacing
    test_file<gil::rgb8_image_t>("S07N3P02.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("S07N3P02.PNG");

    // S08I3P02 - 8x8 paletted file, interlaced
    test_file<gil::rgb8_image_t>("S08I3P02.PNG");

    // S08N3P02 - 8x8 paletted file, no interlacing
    test_file<gil::rgb8_image_t>("S08N3P02.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("S08N3P02.PNG");

    // S09I3P02 - 9x9 paletted file, interlaced
    test_file<gil::rgb8_image_t>("S09I3P02.PNG");

    // S09N3P02 - 9x9 paletted file, no interlacing
    test_file<gil::rgb8_image_t>("S09N3P02.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("S09N3P02.PNG");

    // S32I3P04 - 32x32 paletted file, interlaced
    test_file<gil::rgb8_image_t>("S32I3P04.PNG");

    // S32N3P04 - 32x32 paletted file, no interlacing
    test_file<gil::rgb8_image_t>("S32N3P04.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("S32N3P04.PNG");

    // S33I3P04 - 33x33 paletted file, interlaced
    test_file<gil::rgb8_image_t>("S33I3P04.PNG");

    // S33N3P04 - 33x33 paletted file, no interlacing
    test_file<gil::rgb8_image_t>("S33N3P04.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("S33N3P04.PNG");

    // S34I3P04 - 34x34 paletted file, interlaced
    test_file<gil::rgb8_image_t>("S34I3P04.PNG");

    // S34N3P04 - 34x34 paletted file, no interlacing
    test_file<gil::rgb8_image_t>("S34N3P04.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("S34N3P04.PNG");

    // S35I3P04 - 35x35 paletted file, interlaced
    test_file<gil::rgb8_image_t>("S35I3P04.PNG");

    // S35N3P04 - 35x35 paletted file, no interlacing
    test_file<gil::rgb8_image_t>("S35N3P04.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("S35N3P04.PNG");

    // S36I3P04 - 36x36 paletted file, interlaced
    test_file<gil::rgb8_image_t>("S36I3P04.PNG");

    // S36N3P04 - 36x36 paletted file, no interlacing
    test_file<gil::rgb8_image_t>("S36N3P04.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("S36N3P04.PNG");

    // S37I3P04 - 37x37 paletted file, interlaced
    test_file<gil::rgb8_image_t>("S37I3P04.PNG");

    // S37N3P04 - 37x37 paletted file, no interlacing
    test_file<gil::rgb8_image_t>("S37N3P04.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("S37N3P04.PNG");

    // S38I3P04 - 38x38 paletted file, interlaced
    test_file<gil::rgb8_image_t>("S38I3P04.PNG");

    // S38N3P04 - 38x38 paletted file, no interlacing
    test_file<gil::rgb8_image_t>("S38N3P04.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("S38N3P04.PNG");

    // S39I3P04 - 39x39 paletted file, interlaced
    test_file<gil::rgb8_image_t>("S39I3P04.PNG");

    // S39N3P04 - 39x39 paletted file, no interlacing
    test_file<gil::rgb8_image_t>("S39N3P04.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("S39N3P04.PNG");

    // S40I3P04 - 40x40 paletted file, interlaced
    test_file<gil::rgb8_image_t>("S40I3P04.PNG");

    // S40N3P04 - 40x40 paletted file, no interlacing
    test_file<gil::rgb8_image_t>("S40N3P04.PNG");
    test_png_scanline_reader<gil::rgb8_image_t>("S40N3P04.PNG");
}

void test_background()
{
    // BGAI4A08 - 8 bit grayscale, alpha, no background chunk, interlaced
    test_file<gil::gray_alpha8_image_t>("BGAI4A08.PNG");

    // BGAI4A16 - 16 bit grayscale, alpha, no background chunk, interlaced
    test_file<gil::gray_alpha16_image_t>("BGAI4A16.PNG");

    // BGAN6A08 - 3x8 bits gil::rgb color, alpha, no background chunk
    test_file<gil::rgba8_image_t>("BGAN6A08.PNG");

    // BGAN6A16 - 3x16 bits gil::rgb color, alpha, no background chunk
    test_file<gil::rgba16_image_t>("BGAN6A16.PNG");

    // BGBN4A08 - 8 bit grayscale, alpha, black background chunk
    test_file<gil::gray_alpha8_image_t>("BGBN4A08.PNG");

    // BGGN4A16 - 16 bit grayscale, alpha, gray background chunk
    test_file<gil::gray_alpha16_image_t>("BGGN4A16.PNG");

    // BGWN6A08 - 3x8 bits gil::rgb color, alpha, white background chunk
    test_file<gil::rgba8_image_t>("BGWN6A08.PNG");

    // BGYN6A16 - 3x16 bits gil::rgb color, alpha, yellow background chunk
    test_file<gil::rgba16_image_t>("BGYN6A16.PNG");
}

void test_transparency()
{
    // TBBN1G04 - transparent, black background chunk
    // file missing
    //test_file<gil::gray_alpha8_image_t>("TBBN1G04.PNG" );

    // TBBN2C16 - transparent, blue background chunk
    test_file<gil::rgba16_image_t>("TBBN2C16.PNG");

    // TBBN3P08 - transparent, black background chunk
    test_file<gil::rgba8_image_t>("TBBN3P08.PNG");

    // TBGN2C16 - transparent, green background chunk
    test_file<gil::rgba16_image_t>("TBGN2C16.PNG");

    // TBGN3P08 - transparent, light-gray background chunk
    test_file<gil::rgba8_image_t>("TBGN3P08.PNG");

    // TBRN2C08 - transparent, red background chunk
    test_file<gil::rgba8_image_t>("TBRN2C08.PNG");

    // TBWN1G16 - transparent, white background chunk
    test_file<gil::gray_alpha16_image_t>("TBWN0G16.PNG");

    // TBWN3P08 - transparent, white background chunk
    test_file<gil::rgba8_image_t>("TBWN3P08.PNG");

    // TBYN3P08 - transparent, yellow background chunk
    test_file<gil::rgba8_image_t>("TBYN3P08.PNG");

    // TP0N1G08 - not transparent for reference (logo on gray)
    test_file<gil::gray8_image_t>("TP0N0G08.PNG");

    // TP0N2C08 - not transparent for reference (logo on gray)
    test_file<gil::rgb8_image_t>("TP0N2C08.PNG");

    // TP0N3P08 - not transparent for reference (logo on gray)
    test_file<gil::rgb8_image_t>("TP0N3P08.PNG");

    // TP1N3P08 - transparent, but no background chunk
    test_file<gil::rgba8_image_t>("TP1N3P08.PNG");
}

void test_gamma()
{
    // G03N0G16 - grayscale, file-gamma = 0.35
    test_file<gil::gray16_image_t>("G03N0G16.PNG");

    // G03N2C08 - color, file-gamma = 0.35
    test_file<gil::rgb8_image_t>("G03N2C08.PNG");

    // G03N3P04 - paletted, file-gamma = 0.35
    test_file<gil::rgb8_image_t>("G03N3P04.PNG");

    // G04N0G16 - grayscale, file-gamma = 0.45
    test_file<gil::gray16_image_t>("G04N0G16.PNG");

    // G04N2C08 - color, file-gamma = 0.45
    test_file<gil::rgb8_image_t>("G04N2C08.PNG");

    // G04N3P04 - paletted, file-gamma = 0.45
    test_file<gil::rgb8_image_t>("G04N3P04.PNG");

    // G05N0G16 - grayscale, file-gamma = 0.55
    test_file<gil::gray16_image_t>("G05N0G16.PNG");

    // G05N2C08 - color, file-gamma = 0.55
    test_file<gil::rgb8_image_t>("G05N2C08.PNG");

    // G05N3P04 - paletted, file-gamma = 0.55
    test_file<gil::rgb8_image_t>("G05N3P04.PNG");

    // G07N0G16 - grayscale, file-gamma = 0.70
    test_file<gil::gray16_image_t>("G07N0G16.PNG");

    // G07N2C08 - color, file-gamma = 0.70
    test_file<gil::rgb8_image_t>("G07N2C08.PNG");

    // G07N3P04 - paletted, file-gamma = 0.70
    test_file<gil::rgb8_image_t>("G07N3P04.PNG");

    // G10N0G16 - grayscale, file-gamma = 1.00
    test_file<gil::gray16_image_t>("G10N0G16.PNG");

    // G10N2C08 - color, file-gamma = 1.00
    test_file<gil::rgb8_image_t>("G10N2C08.PNG");

    // G10N3P04 - paletted, file-gamma = 1.00
    test_file<gil::rgb8_image_t>("G10N3P04.PNG");

    // G25N0G16 - grayscale, file-gamma = 2.50
    test_file<gil::gray16_image_t>("G25N0G16.PNG");

    // G25N2C08 - color, file-gamma = 2.50
    test_file<gil::rgb8_image_t>("G25N2C08.PNG");

    // G25N3P04 - paletted, file-gamma = 2.50
    test_file<gil::rgb8_image_t>("G25N3P04.PNG");
}
#endif // BOOST_GIL_IO_USE_PNG_TEST_SUITE_IMAGES

void test_corrupted_png_read()
{
// taken from https://github.com/boostorg/gil/issues/401#issue-518615480
// author: https://github.com/misos1

	std::initializer_list<unsigned char> corrupt_png = {
		0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A,
		0x00, 0x00, 0x00, 0x0D,
		'I', 'H', 'D', 'R',
		0x00, 0x00, 0x04, 0x00,
		0x00, 0x00, 0x05, 0xA9,
		0x08, 0x02, 0x00, 0x00, 0x00,
		0x68, 0x1B, 0xF7, 0x46,
		0x00, 0x00, 0x00, 0x00,
		'I', 'D', 'A', 'T',
		0x35, 0xAF, 0x06, 0x1E,
		0x00, 0x00, 0x00, 0x00,
		'I', 'E', 'N', 'D',
		0xAE, 0x42, 0x60, 0x82
	};

    std::stringstream ss(
        std::string(corrupt_png.begin(), corrupt_png.end()),
        std::ios_base::in | std::ios_base::binary);
    boost::gil::rgb8_image_t img;

    // TODO: Replace with BOOST_ERROR below with BOOST_TEST_THROWS
    try
    {
        boost::gil::read_image(ss, img, boost::gil::png_tag{});
    }
    catch (std::ios_base::failure& exception)
    {
        // the exception message is "png is invalid: iostream error"
        // is the error portable throughout stdlib implementations,
        // or is it native to this one?
        // the description passd is "png_is_invalid"
        // the exact error message is thus not checked
        return;
    }

    BOOST_ERROR("no exception was thrown, which is an error");
}

int main()
{
    test_read_header();
    test_corrupted_png_read();

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
    test_read_pixel_per_meter();
    test_read_with_trns_chunk_color_type_0();
    test_read_with_trns_chunk_color_type_2();
    test_read_with_trns_chunk_color_type_3();
#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

#ifdef BOOST_GIL_IO_USE_PNG_TEST_SUITE_IMAGES
    test_basic_format();
    test_basic_format_interlaced();
    test_odd_sizes();
    test_background();
    test_transparency();
    test_gamma();
#endif

    return boost::report_errors();
}

