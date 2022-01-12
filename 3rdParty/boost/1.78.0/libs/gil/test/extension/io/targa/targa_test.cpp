//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#define BOOST_GIL_IO_ADD_FS_PATH_SUPPORT
#include <boost/gil.hpp>
#include <boost/gil/extension/io/targa.hpp>

#include <boost/mp11.hpp>
#include <boost/core/lightweight_test.hpp>

#include <fstream>
#include <sstream>
#include <string>

#include "mandel_view.hpp"
#include "paths.hpp"
#include "subimage_test.hpp"
#include "test_utility_output_stream.hpp"

namespace fs = boost::filesystem;
namespace gil = boost::gil;
namespace mp11 = boost::mp11;

void test_read_image_info_using_string()
{
    {
        using backend_t   = gil::get_reader_backend<std::string const, gil::targa_tag>::type;
        backend_t backend = gil::read_image_info(targa_filename, gil::targa_tag());

        BOOST_TEST_EQ(backend._info._width, 124);
        BOOST_TEST_EQ(backend._info._height, 124);
    }
    {
        std::ifstream in(targa_filename.c_str(), std::ios::binary);

        using backend_t   = gil::get_reader_backend<std::ifstream, gil::targa_tag>::type;
        backend_t backend = gil::read_image_info(in, gil::targa_tag());

        BOOST_TEST_EQ(backend._info._width, 124);
        BOOST_TEST_EQ(backend._info._height, 124);
    }
    {
        FILE* file = fopen(targa_filename.c_str(), "rb");

        using backend_t   = gil::get_reader_backend<FILE*, gil::targa_tag>::type;
        backend_t backend = gil::read_image_info(file, gil::targa_tag());

        BOOST_TEST_EQ(backend._info._width, 124);
        BOOST_TEST_EQ(backend._info._height, 124);
    }
    {
        fs::path my_path(targa_filename);

        using backend_t   = gil::get_reader_backend<fs::path, gil::targa_tag>::type;
        backend_t backend = gil::read_image_info(my_path, gil::targa_tag());

        BOOST_TEST_EQ(backend._info._width, 124);
        BOOST_TEST_EQ(backend._info._height, 124);
    }
}

void test_read_image()
{
    {
        gil::rgb8_image_t img;
        gil::read_image(targa_filename, img, gil::targa_tag());

        BOOST_TEST_EQ(img.width(), 124);
        BOOST_TEST_EQ(img.height(), 124);
    }
    {
        std::ifstream in(targa_filename.c_str(), std::ios::binary);

        gil::rgb8_image_t img;
        gil::read_image(in, img, gil::targa_tag());

        BOOST_TEST_EQ(img.width(), 124);
        BOOST_TEST_EQ(img.height(), 124);
    }
    {
        FILE* file = fopen(targa_filename.c_str(), "rb");

        gil::rgb8_image_t img;
        gil::read_image(file, img, gil::targa_tag());

        BOOST_TEST_EQ(img.width(), 124);
        BOOST_TEST_EQ(img.height(), 124);
    }
}

void test_read_and_convert_image()
{
    {
        gil::rgb8_image_t img;
        read_and_convert_image(targa_filename, img, gil::targa_tag());

        BOOST_TEST_EQ(img.width(), 124);
        BOOST_TEST_EQ(img.height(), 124);
    }
    {
        std::ifstream in(targa_filename.c_str(), std::ios::binary);

        gil::rgb8_image_t img;
        gil::read_and_convert_image(in, img, gil::targa_tag());

        BOOST_TEST_EQ(img.width(), 124);
        BOOST_TEST_EQ(img.height(), 124);
    }
    {
        FILE* file = fopen(targa_filename.c_str(), "rb");

        gil::rgb8_image_t img;
        gil::read_and_convert_image(file, img, gil::targa_tag());

        BOOST_TEST_EQ(img.width(), 124);
        BOOST_TEST_EQ(img.height(), 124);
    }
}

void test_read_view()
{
    {
        gil::rgb8_image_t img(124, 124);
        gil::read_view(targa_filename, gil::view(img), gil::targa_tag());
    }
    {
        std::ifstream in(targa_filename.c_str(), std::ios::binary);

        gil::rgb8_image_t img(124, 124);
        gil::read_view(in, gil::view(img), gil::targa_tag());
    }
    {
        FILE* file = fopen(targa_filename.c_str(), "rb");

        gil::rgb8_image_t img(124, 124);
        gil::read_view(file, gil::view(img), gil::targa_tag());
    }
}

void test_read_and_convert_view()
{
    {
        gil::rgb8_image_t img(124, 124);
        gil::read_and_convert_view(targa_filename, gil::view(img), gil::targa_tag());
    }
    {
        std::ifstream in(targa_filename.c_str(), std::ios::binary);

        gil::rgb8_image_t img(124, 124);
        gil::read_and_convert_view(in, gil::view(img), gil::targa_tag());
    }
    {
        FILE* file = fopen(targa_filename.c_str(), "rb");

        gil::rgb8_image_t img(124, 124);
        gil::read_and_convert_view(file, gil::view(img), gil::targa_tag());
    }
}

void test_write_view()
{
    {
        std::string filename(png_out + "write_test_string.tga");

        gil::rgb8_image_t img;
        gil::write_view(
            filename,
            create_mandel_view(124, 124, gil::rgb8_pixel_t(0, 0, 255), gil::rgb8_pixel_t(0, 255, 0)),
            gil::targa_tag());
    }
    {
        std::string filename(targa_out + "write_test_ofstream.tga");
        std::ofstream out(filename.c_str(), std::ios::out | std::ios::binary);

        gil::write_view(
            out,
            create_mandel_view(124, 124, gil::rgb8_pixel_t(0, 0, 255), gil::rgb8_pixel_t(0, 255, 0)),
            gil::targa_tag());
    }
    {
        std::string filename(targa_out + "write_test_file.tga");
        FILE* file = fopen(filename.c_str(), "wb");

        gil::write_view(
            file,
            create_mandel_view(124, 124, gil::rgb8_pixel_t(0, 0, 255), gil::rgb8_pixel_t(0, 255, 0)),
            gil::targa_tag());
    }
    {
        std::string filename(targa_out + "write_test_info.tga");
        FILE* file = fopen(filename.c_str(), "wb");

        gil::image_write_info<gil::targa_tag> info;
        gil::write_view(
            file,
            create_mandel_view(124, 124, gil::rgb8_pixel_t(0, 0, 255), gil::rgb8_pixel_t(0, 255, 0)),
            info);
    }
}

void test_stream()
{
    // 1. Read an image.
    std::ifstream in(targa_filename.c_str(), std::ios::binary);

    gil::rgb8_image_t img;
    gil::read_image(in, img, gil::targa_tag());

    // 2. Write image to in-memory buffer.
    std::stringstream out_buffer(std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    gil::write_view(out_buffer, gil::view(img), gil::targa_tag());

    // 3. Copy in-memory buffer to another.
    std::stringstream in_buffer(std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    in_buffer << out_buffer.rdbuf();

    // 4. Read in-memory buffer to gil image
    gil::rgb8_image_t dst;
    gil::read_image(in_buffer, dst, gil::targa_tag());

    // 5. Write out image.
    std::string filename(targa_out + "stream_test.tga");
    std::ofstream out(filename.c_str(), std::ios_base::binary);
    gil::write_view(out, gil::view(dst), gil::targa_tag());
}

void test_stream_2()
{
    std::filebuf in_buf;
    if (!in_buf.open(targa_filename.c_str(), std::ios::in | std::ios::binary))
    {
        BOOST_TEST(false);
    }

    std::istream in(&in_buf);

    gil::rgb8_image_t img;
    gil::read_image(in, img, gil::targa_tag());
}

void test_subimage()
{
    run_subimage_test<gil::rgb8_image_t, gil::targa_tag>(
        targa_filename, gil::point_t(0, 0), gil::point_t(50, 50));

    // FIXME: not working
    // run_subimage_test<gil::gray8_image_t, gil::targa_tag>(
    //     targa_filename, gil::point_t(39, 7), gil::point_t(50, 50));
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
    gil::read_image(targa_filename.c_str(), image, gil::targa_tag());
    gil::write_view(targa_out + "dynamic_image_test.tga", gil::view(image), gil::targa_tag());
}

int main()
{
    test_read_image_info_using_string();
    test_read_image();
    test_read_and_convert_image();
    test_read_view();
    test_read_and_convert_view();
    test_stream();
    test_stream_2();
    test_subimage();
    test_dynamic_image();

    return boost::report_errors();
}
