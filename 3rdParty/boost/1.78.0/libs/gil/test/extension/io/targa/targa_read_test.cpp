//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/io/targa.hpp>

#include <boost/core/lightweight_test.hpp>

#include "paths.hpp"
#include "scanline_read_test.hpp"
#include "test_utility_output_stream.hpp"

namespace gil = boost::gil;

template <typename Image>
void test_targa_scanline_reader(std::string filename)
{
    test_scanline_reader<Image, gil::targa_tag>(std::string(targa_in + filename).c_str());
}

template <typename Image>
void write(Image& img, std::string const& file_name)
{
    gil::write_view(targa_out + file_name, gil::view(img), gil::targa_tag());
}

void test_read_header()
{
    using backend_t   = gil::get_reader_backend<std::string const, gil::targa_tag>::type;
    backend_t backend = gil::read_image_info(targa_filename, gil::targa_tag());

    BOOST_TEST_EQ(backend._info._header_size, 18);
    BOOST_TEST_EQ(backend._info._offset, 18);
    BOOST_TEST_EQ(backend._info._color_map_type, 0);
    BOOST_TEST_EQ(backend._info._image_type, 10);
    BOOST_TEST_EQ(backend._info._color_map_start, 0);
    BOOST_TEST_EQ(backend._info._color_map_length, 0);
    BOOST_TEST_EQ(backend._info._color_map_depth, 0);
    BOOST_TEST_EQ(backend._info._x_origin, 0);
    BOOST_TEST_EQ(backend._info._y_origin, 0);
    BOOST_TEST_EQ(backend._info._width, 124);
    BOOST_TEST_EQ(backend._info._height, 124);
    BOOST_TEST_EQ(backend._info._bits_per_pixel, 24);
    BOOST_TEST_EQ(backend._info._descriptor, 0);
}

void test_read_reference_images()
{
    // 24BPP_compressed.tga
    {
        gil::rgb8_image_t img;
        gil::read_image(targa_in + "24BPP_compressed.tga", img, gil::targa_tag());

        typename gil::rgb8_image_t::x_coord_t width  = gil::view(img).width();
        typename gil::rgb8_image_t::y_coord_t height = gil::view(img).height();

        BOOST_TEST_EQ(width, 124);
        BOOST_TEST_EQ(height, 124);
        BOOST_TEST_EQ(gil::view(img)(0, 0), gil::rgb8_pixel_t(248, 0, 248));
        BOOST_TEST_EQ(gil::view(img)(width - 1, 0), gil::rgb8_pixel_t(0, 0, 248));
        BOOST_TEST_EQ(gil::view(img)(0, height - 1), gil::rgb8_pixel_t(248, 0, 0));
        BOOST_TEST_EQ(gil::view(img)(width - 1, height - 1), gil::rgb8_pixel_t(248, 0, 248));

        write(img, "24BPP_compressed_out.tga");
    }
    // 24BPP_uncompressed.tga
    {
        gil::rgb8_image_t img;
        gil::read_image(targa_in + "24BPP_uncompressed.tga", img, gil::targa_tag());

        typename gil::rgb8_image_t::x_coord_t width  = gil::view(img).width();
        typename gil::rgb8_image_t::y_coord_t height = gil::view(img).height();

        BOOST_TEST_EQ(width, 124);
        BOOST_TEST_EQ(height, 124);
        BOOST_TEST_EQ(gil::view(img)(0, 0), gil::rgb8_pixel_t(248, 0, 248));
        BOOST_TEST_EQ(gil::view(img)(width - 1, 0), gil::rgb8_pixel_t(0, 0, 248));
        BOOST_TEST_EQ(gil::view(img)(0, height - 1), gil::rgb8_pixel_t(248, 0, 0));
        BOOST_TEST_EQ(gil::view(img)(width - 1, height - 1), gil::rgb8_pixel_t(248, 0, 248));

        write(img, "24BPP_uncompressed_out.tga");

        test_targa_scanline_reader<gil::bgr8_image_t>("24BPP_uncompressed.tga");
    }
    // 32BPP_compressed.tga
    {
        gil::rgba8_image_t img;
        gil::read_image(targa_in + "32BPP_compressed.tga", img, gil::targa_tag());

        typename gil::rgba8_image_t::x_coord_t width  = gil::view(img).width();
        typename gil::rgba8_image_t::y_coord_t height = gil::view(img).height();

        BOOST_TEST_EQ(width, 124);
        BOOST_TEST_EQ(height, 124);
        BOOST_TEST_EQ(gil::view(img)(0, 0), gil::rgba8_pixel_t(248, 0, 248, 255));
        BOOST_TEST_EQ(gil::view(img)(width - 1, 0), gil::rgba8_pixel_t(0, 0, 248, 255));
        BOOST_TEST_EQ(gil::view(img)(0, height - 1), gil::rgba8_pixel_t(0, 0, 0, 0));
        BOOST_TEST_EQ(gil::view(img)(width - 1, height - 1), gil::rgba8_pixel_t(248, 0, 248, 255));

        write(img, "32BPP_compressed_out.tga");
    }
    // 32BPP_uncompressed.tga
    {
        gil::rgba8_image_t img;
        gil::read_image(targa_in + "32BPP_uncompressed.tga", img, gil::targa_tag());

        typename gil::rgba8_image_t::x_coord_t width  = gil::view(img).width();
        typename gil::rgba8_image_t::y_coord_t height = gil::view(img).height();

        BOOST_TEST_EQ(width, 124);
        BOOST_TEST_EQ(height, 124);
        BOOST_TEST_EQ(gil::view(img)(0, 0), gil::rgba8_pixel_t(248, 0, 248, 255));
        BOOST_TEST_EQ(gil::view(img)(width - 1, 0), gil::rgba8_pixel_t(0, 0, 248, 255));
        BOOST_TEST_EQ(gil::view(img)(0, height - 1), gil::rgba8_pixel_t(0, 0, 0, 0));
        BOOST_TEST_EQ(gil::view(img)(width - 1, height - 1), gil::rgba8_pixel_t(248, 0, 248, 255));

        write(img, "32BPP_uncompressed_out.tga");

        test_targa_scanline_reader<gil::bgra8_image_t>("32BPP_uncompressed.tga");
    }
    // 24BPP_compressed_ul_origin.tga
    {
        gil::rgb8_image_t img;
        gil::read_image(targa_in + "24BPP_compressed_ul_origin.tga", img, gil::targa_tag());

        typename gil::rgb8_image_t::x_coord_t width  = gil::view(img).width();
        typename gil::rgb8_image_t::y_coord_t height = gil::view(img).height();

        BOOST_TEST_EQ(width, 124);
        BOOST_TEST_EQ(height, 124);
        BOOST_TEST_EQ(gil::view(img)(0, 0), gil::rgb8_pixel_t(248, 0, 248));
        BOOST_TEST_EQ(gil::view(img)(width - 1, 0), gil::rgb8_pixel_t(0, 0, 248));
        BOOST_TEST_EQ(gil::view(img)(0, height - 1), gil::rgb8_pixel_t(248, 0, 0));
        BOOST_TEST_EQ(gil::view(img)(width - 1, height - 1), gil::rgb8_pixel_t(248, 0, 248));

        write(img, "24BPP_compressed_ul_origin_out.tga");
    }
    // 24BPP_uncompressed_ul_origin.tga
    {
        gil::rgb8_image_t img;
        gil::read_image(targa_in + "24BPP_uncompressed_ul_origin.tga", img, gil::targa_tag());

        typename gil::rgb8_image_t::x_coord_t width  = gil::view(img).width();
        typename gil::rgb8_image_t::y_coord_t height = gil::view(img).height();

        BOOST_TEST_EQ(width, 124);
        BOOST_TEST_EQ(height, 124);
        BOOST_TEST_EQ(gil::view(img)(0, 0), gil::rgb8_pixel_t(248, 0, 248));
        BOOST_TEST_EQ(gil::view(img)(width - 1, 0), gil::rgb8_pixel_t(0, 0, 248));
        BOOST_TEST_EQ(gil::view(img)(0, height - 1), gil::rgb8_pixel_t(248, 0, 0));
        BOOST_TEST_EQ(gil::view(img)(width - 1, height - 1), gil::rgb8_pixel_t(248, 0, 248));

        write(img, "24BPP_uncompressed_ul_origin_out.tga");
    }
    // 32BPP_compressed_ul_origin.tga
    {
        gil::rgba8_image_t img;
        gil::read_image(targa_in + "32BPP_compressed_ul_origin.tga", img, gil::targa_tag());

        typename gil::rgba8_image_t::x_coord_t width  = gil::view(img).width();
        typename gil::rgba8_image_t::y_coord_t height = gil::view(img).height();

        BOOST_TEST_EQ(width, 124);
        BOOST_TEST_EQ(height, 124);
        BOOST_TEST_EQ(gil::view(img)(0, 0), gil::rgba8_pixel_t(248, 0, 248, 255));
        BOOST_TEST_EQ(gil::view(img)(width - 1, 0), gil::rgba8_pixel_t(0, 0, 248, 255));
        BOOST_TEST_EQ(gil::view(img)(0, height - 1), gil::rgba8_pixel_t(0, 0, 0, 0));
        BOOST_TEST_EQ(gil::view(img)(width - 1, height - 1), gil::rgba8_pixel_t(248, 0, 248, 255));

        write(img, "32BPP_compressed_ul_origin_out.tga");
    }
    // 32BPP_uncompressed_ul_origin.tga
    {
        gil::rgba8_image_t img;
        gil::read_image(targa_in + "32BPP_uncompressed_ul_origin.tga", img, gil::targa_tag());

        typename gil::rgba8_image_t::x_coord_t width  = gil::view(img).width();
        typename gil::rgba8_image_t::y_coord_t height = gil::view(img).height();

        BOOST_TEST_EQ(width, 124);
        BOOST_TEST_EQ(height, 124);
        BOOST_TEST_EQ(gil::view(img)(0, 0), gil::rgba8_pixel_t(248, 0, 248, 255));
        BOOST_TEST_EQ(gil::view(img)(width - 1, 0), gil::rgba8_pixel_t(0, 0, 248, 255));
        BOOST_TEST_EQ(gil::view(img)(0, height - 1), gil::rgba8_pixel_t(0, 0, 0, 0));
        BOOST_TEST_EQ(gil::view(img)(width - 1, height - 1), gil::rgba8_pixel_t(248, 0, 248, 255));

        write(img, "32BPP_uncompressed_ul_origin_out.tga");
    }
}

void test_partial_image()
{
    std::string const filename(targa_in + "24BPP_compressed.tga");

    gil::rgb8_image_t img;
    gil::read_image(
        filename, img,
        gil::image_read_settings<gil::targa_tag>(gil::point_t(0, 0), gil::point_t(50, 50)));

    gil::write_view(targa_out + "targa_partial.tga", gil::view(img), gil::targa_tag());
}

int main()
{
    test_read_header();
    test_read_reference_images();

    return boost::report_errors();
}
