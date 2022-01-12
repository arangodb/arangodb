//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#define BOOST_GIL_IO_ADD_FS_PATH_SUPPORT
#define BOOST_FILESYSTEM_VERSION 3
#include <boost/gil.hpp>
#include <boost/gil/extension/io/bmp.hpp>

#include <boost/mp11.hpp>
#include <boost/core/lightweight_test.hpp>

#include <fstream>
#include <sstream>
#include <string>

#include "mandel_view.hpp"
#include "paths.hpp"
#include "subimage_test.hpp"

namespace fs = boost::filesystem;
namespace gil = boost::gil;
namespace mp11 = boost::mp11;

void test_read_image_info_using_string()
{
    {
        using backend_t = gil::get_reader_backend<std::string const, gil::bmp_tag>::type;
        backend_t backend = read_image_info(bmp_filename, gil::bmp_tag());

        BOOST_TEST_EQ(backend._info._width, 1000);
        BOOST_TEST_EQ(backend._info._height, 600);
    }
    {
        std::ifstream in(bmp_filename.c_str(), std::ios::binary);
        using backend_t = gil::get_reader_backend<std::ifstream, gil::bmp_tag>::type;
        backend_t backend = read_image_info(in, gil::bmp_tag());

        BOOST_TEST_EQ(backend._info._width, 1000);
        BOOST_TEST_EQ(backend._info._height, 600);
    }
    {
        FILE *file = fopen(bmp_filename.c_str(), "rb");
        using backend_t = gil::get_reader_backend<FILE *, gil::bmp_tag>::type;
        backend_t backend = read_image_info(file, gil::bmp_tag());

        BOOST_TEST_EQ(backend._info._width, 1000);
        BOOST_TEST_EQ(backend._info._height, 600);
    }
    {
        fs::path my_path(bmp_filename);
        using backend_t = gil::get_reader_backend<fs::path, gil::bmp_tag>::type;
        backend_t backend = read_image_info(my_path, gil::bmp_tag());

        BOOST_TEST_EQ(backend._info._width, 1000);
        BOOST_TEST_EQ(backend._info._height, 600);
    }
}

void test_read_image()
{
    {
        gil::rgb8_image_t img;
        read_image(bmp_filename, img, gil::bmp_tag());

        BOOST_TEST_EQ(img.width(), 1000);
        BOOST_TEST_EQ(img.height(), 600);
    }
    {
        std::ifstream in(bmp_filename.c_str(), std::ios::binary);

        gil::rgb8_image_t img;
        read_image(in, img, gil::bmp_tag());

        BOOST_TEST_EQ(img.width(), 1000);
        BOOST_TEST_EQ(img.height(), 600);
    }
    {
        FILE *file = fopen(bmp_filename.c_str(), "rb");

        gil::rgb8_image_t img;
        read_image(file, img, gil::bmp_tag());

        BOOST_TEST_EQ(img.width(), 1000);
        BOOST_TEST_EQ(img.height(), 600);
    }
    {
        fs::path my_path(bmp_filename);

        gil::rgb8_image_t img;
        read_image(my_path, img, gil::bmp_tag());

        BOOST_TEST_EQ(img.width(), 1000);
        BOOST_TEST_EQ(img.height(), 600);
    }
}

void test_read_and_convert_image()
{
    {
        gil::rgb8_image_t img;
        read_and_convert_image(bmp_filename, img, gil::bmp_tag());

        BOOST_TEST_EQ(img.width(), 1000);
        BOOST_TEST_EQ(img.height(), 600);
    }
    {
        std::ifstream in(bmp_filename.c_str(), std::ios::binary);

        gil::rgb8_image_t img;
        read_and_convert_image(in, img, gil::bmp_tag());

        BOOST_TEST_EQ(img.width(), 1000);
        BOOST_TEST_EQ(img.height(), 600);
    }
    {
        FILE *file = fopen(bmp_filename.c_str(), "rb");

        gil::rgb8_image_t img;
        read_and_convert_image(file, img, gil::bmp_tag());

        BOOST_TEST_EQ(img.width(), 1000);
        BOOST_TEST_EQ(img.height(), 600);
    }
}

void test_read_view()
{
    {
        gil::rgb8_image_t img(1000, 600);
        read_view(bmp_filename, gil::view(img), gil::bmp_tag());
    }
    {
        std::ifstream in(bmp_filename.c_str(), std::ios::binary);

        gil::rgb8_image_t img(1000, 600);
        read_view(in, gil::view(img), gil::bmp_tag());
    }
    {
        FILE *file = fopen(bmp_filename.c_str(), "rb");

        gil::rgb8_image_t img(1000, 600);
        read_view(file, gil::view(img), gil::bmp_tag());
    }
}

void test_read_and_convert_view()
{
    {
        gil::rgb8_image_t img(1000, 600);
        read_and_convert_view(bmp_filename, gil::view(img), gil::bmp_tag());
    }
    {
        std::ifstream in(bmp_filename.c_str(), std::ios::binary);

        gil::rgb8_image_t img(1000, 600);
        read_and_convert_view(in, gil::view(img), gil::bmp_tag());
    }
    {
        FILE *file = fopen(bmp_filename.c_str(), "rb");

        gil::rgb8_image_t img(1000, 600);
        read_and_convert_view(file, gil::view(img), gil::bmp_tag());
    }
}

void test_write_view()
{
    auto const b = gil::rgb8_pixel_t(0, 0, 255);
    auto const g = gil::rgb8_pixel_t(0, 255, 0);
    {
        std::string filename(bmp_out + "write_test_string.bmp");
        gil::write_view(filename, create_mandel_view(1000, 600, b, g), gil::bmp_tag());
    }
    {
        std::string filename(bmp_out + "write_test_ofstream.bmp");
        std::ofstream out(filename.c_str(), std::ios::binary);

        gil::write_view(out, create_mandel_view(1000, 600, b, g), gil::bmp_tag());
    }
    {
        std::string filename(bmp_out + "write_test_file.bmp");

        FILE *file = fopen(filename.c_str(), "wb");
        gil::write_view(file, create_mandel_view(1000, 600, b, g), gil::bmp_tag());
    }
    {
        std::string filename(bmp_out + "write_test_info.bmp");

        gil::image_write_info<gil::bmp_tag> info;
        FILE *file = fopen(filename.c_str(), "wb");
        gil::write_view(file, create_mandel_view(1000, 600, b, g), info);
    }
}

void test_stream()
{
    // 1. Read an image.
    std::ifstream in(bmp_filename.c_str(), std::ios::binary);

    gil::rgb8_image_t img;
    gil::read_image(in, img, gil::bmp_tag());

    // 2. Write image to in-memory buffer.
    std::stringstream out_buffer(std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    gil::write_view(out_buffer, gil::view(img), gil::bmp_tag());

    // 3. Copy in-memory buffer to another.
    std::stringstream in_buffer(std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    in_buffer << out_buffer.rdbuf();

    // 4. Read in-memory buffer to gil image
    gil::rgb8_image_t dst;
    gil::read_image(in_buffer, dst, gil::bmp_tag());

    // 5. Write out image.
    std::string filename(bmp_out + "stream_test.bmp");
    std::ofstream out(filename.c_str(), std::ios_base::binary);

    gil::write_view( out, gil::view( dst ), gil::bmp_tag() );
}

void test_stream_2()
{
    std::filebuf in_buf;
    if (!in_buf.open(bmp_filename.c_str(), std::ios::in | std::ios::binary))
    {
        BOOST_TEST(false);
    }
    std::istream in(&in_buf);

    gil::rgb8_image_t img;
    read_image(in, img, gil::bmp_tag());
}

void test_subimage()
{
    run_subimage_test<gil::rgb8_image_t, gil::bmp_tag>(
        bmp_filename, gil::point_t(0, 0), gil::point_t(1000, 1));

    run_subimage_test<gil::rgb8_image_t, gil::bmp_tag>(
        bmp_filename, gil::point_t(39, 7), gil::point_t(50, 50));
}

void test_dynamic_image()
{
    using my_img_types = mp11::mp_list
        <
            gil::gray8_image_t,
            gil::gray16_image_t,
            gil::rgb8_image_t,
            gil::rgba8_image_t
        >;

    gil::any_image<my_img_types> image;
    gil::read_image(bmp_filename.c_str(), image, gil::bmp_tag());

    gil::write_view(bmp_out + "dynamic_image_test.bmp", gil::view(image), gil::bmp_tag());
}

int main(int argc, char *argv[])
{
    try
    {
        test_read_image_info_using_string();
        test_read_image();
        test_read_and_convert_image();
        test_read_view();
        test_read_and_convert_view();
        test_write_view();
        test_stream();
        test_stream_2();
        test_subimage();
        test_dynamic_image();
    }
    catch (std::exception const& e)
    {
        BOOST_ERROR(e.what());
    }
    return boost::report_errors();
}
