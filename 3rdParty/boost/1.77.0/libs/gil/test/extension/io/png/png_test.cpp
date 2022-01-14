//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
//#define BOOST_GIL_IO_PNG_FLOATING_POINT_SUPPORTED
//#define BOOST_GIL_IO_PNG_FIXED_POINT_SUPPORTED
#include <boost/gil.hpp>
#include <boost/gil/extension/io/png.hpp>

#include <boost/mp11.hpp>
#include <boost/core/lightweight_test.hpp>

#include <fstream>
#include <sstream>
#include <string>

#include "mandel_view.hpp"
#include "paths.hpp"
#include "subimage_test.hpp"

namespace gil  = boost::gil;
namespace mp11 = boost::mp11;

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
void test_read_image_info_using_string()
{
    {
        using backend_t   = gil::get_reader_backend<std::string const, gil::png_tag>::type;
        backend_t backend = gil::read_image_info(png_filename, gil::png_tag());

        BOOST_TEST_EQ(backend._info._width, 1000u);
        BOOST_TEST_EQ(backend._info._height, 600u);
    }
    {
        std::ifstream in(png_filename.c_str(), std::ios::binary);

        using backend_t = gil::get_reader_backend<std::ifstream, gil::png_tag>::type;
        backend_t backend = gil::read_image_info(in, gil::png_tag());

        BOOST_TEST_EQ(backend._info._width, 1000u);
        BOOST_TEST_EQ(backend._info._height, 600u);
    }
    {
        FILE* file = fopen(png_filename.c_str(), "rb");

        using backend_t = gil::get_reader_backend<FILE*, gil::png_tag>::type;
        backend_t backend = gil::read_image_info(file, gil::png_tag());

        BOOST_TEST_EQ(backend._info._width, 1000u);
        BOOST_TEST_EQ(backend._info._height, 600u);
    }
}

void test_read_image()
{
    {
        gil::rgba8_image_t img;
        gil::read_image(png_filename, img, gil::png_tag());

        BOOST_TEST_EQ(img.width(), 1000u);
        BOOST_TEST_EQ(img.height(), 600u);
    }
    {
        std::ifstream in(png_filename.c_str(), std::ios::binary);

        gil::rgba8_image_t img;
        gil::read_image(in, img, gil::png_tag());

        BOOST_TEST_EQ(img.width(), 1000u);
        BOOST_TEST_EQ(img.height(), 600u);
    }
    {
        FILE* file = fopen(png_filename.c_str(), "rb");

        gil::rgba8_image_t img;
        gil::read_image(file, img, gil::png_tag());

        BOOST_TEST_EQ(img.width(), 1000u);
        BOOST_TEST_EQ(img.height(), 600u);
    }
}

void test_read_and_convert_image()
{
    {
        gil::rgb8_image_t img;
        gil::read_and_convert_image(png_filename, img, gil::png_tag());

        BOOST_TEST_EQ(img.width(), 1000u);
        BOOST_TEST_EQ(img.height(), 600u);
    }
    {
        gil::rgba8_image_t img;
        gil::read_and_convert_image(png_filename, img, gil::png_tag());

        BOOST_TEST_EQ(img.width(), 1000u);
        BOOST_TEST_EQ(img.height(), 600u);
    }
    {
        std::ifstream in(png_filename.c_str(), std::ios::binary);

        gil::rgb8_image_t img;
        gil::read_and_convert_image(in, img, gil::png_tag());

        BOOST_TEST_EQ(img.width(), 1000u);
        BOOST_TEST_EQ(img.height(), 600u);
    }
    {
        FILE* file = fopen(png_filename.c_str(), "rb");

        gil::rgb8_image_t img;
        gil::read_and_convert_image(file, img, gil::png_tag());

        BOOST_TEST_EQ(img.width(), 1000u);
        BOOST_TEST_EQ(img.height(), 600u);
    }
}

void test_read_view()
{
    {
        gil::rgba8_image_t img(1000, 600);
        gil::read_view(png_filename, view(img), gil::png_tag());
    }
    {
        std::ifstream in(png_filename.c_str(), std::ios::binary);

        gil::rgba8_image_t img(1000, 600);
        gil::read_view(in, view(img), gil::png_tag());
    }
    {
        FILE* file = fopen(png_filename.c_str(), "rb");

        gil::rgba8_image_t img(1000, 600);
        gil::read_view(file, view(img), gil::png_tag());
    }
}

void test_read_and_convert_view()
{
    {
        gil::rgb8_image_t img(1000, 600);
        gil::read_and_convert_view(png_filename, view(img), gil::png_tag());
    }
    {
        std::ifstream in(png_filename.c_str(), std::ios::binary);

        gil::rgb8_image_t img(1000, 600);
        gil::read_and_convert_view(in, view(img), gil::png_tag());
    }
    {
        FILE* file = fopen(png_filename.c_str(), "rb");

        gil::rgb8_image_t img(1000, 600);
        gil::read_and_convert_view(file, view(img), gil::png_tag());
    }
}
#endif  // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
void test_write_view()
{
    {
        std::string filename(png_out + "write_test_string.png");

        gil::write_view(
            filename,
            create_mandel_view(320, 240, gil::rgb8_pixel_t(0, 0, 255), gil::rgb8_pixel_t(0, 255, 0)),
            gil::png_tag());
    }
    {
        std::string filename(png_out + "write_test_string_bgr.png");

        gil::write_view(
            filename,
            create_mandel_view(320, 240, gil::bgr8_pixel_t(255, 0, 0), gil::bgr8_pixel_t(0, 255, 0)),
            gil::png_tag());
    }
    {
        std::string filename(png_out + "write_test_ofstream.png");
        std::ofstream out(filename.c_str(), std::ios::out | std::ios::binary);

        gil::write_view(
            out,
            create_mandel_view(320, 240, gil::rgb8_pixel_t(0, 0, 255), gil::rgb8_pixel_t(0, 255, 0)),
            gil::png_tag());
    }
    {
        std::string filename(png_out + "write_test_file.png");
        FILE* file = fopen(filename.c_str(), "wb");

        gil::write_view(
            file,
            create_mandel_view(320, 240, gil::rgb8_pixel_t(0, 0, 255), gil::rgb8_pixel_t(0, 255, 0)),
            gil::png_tag());
    }

    {
        std::string filename(png_out + "write_test_info.png");
        FILE* file = fopen(filename.c_str(), "wb");

        gil::image_write_info<gil::png_tag> info;
        gil::write_view(
            file,
            create_mandel_view(320, 240, gil::rgb8_pixel_t(0, 0, 255), gil::rgb8_pixel_t(0, 255, 0)),
            info);
    }
}
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
void test_stream()
{
    // 1. Read an image.
    std::ifstream in(png_filename.c_str(), std::ios::binary);

    gil::rgba8_image_t img;
    gil::read_image(in, img, gil::png_tag());

    // 2. Write image to in-memory buffer.
    std::stringstream out_buffer(std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    gil::write_view(out_buffer, view(img), gil::png_tag());

    // 3. Copy in-memory buffer to another.
    std::stringstream in_buffer(std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    in_buffer << out_buffer.rdbuf();

    // 4. Read in-memory buffer to gil image
    gil::rgba8_image_t dst;
    gil::read_image(in_buffer, dst, gil::png_tag());

    // 5. Write out image.
    std::string filename(png_out + "stream_test.png");
    std::ofstream out(filename.c_str(), std::ios_base::binary);

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(out, view(dst), gil::png_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

void test_stream_2()
{
    std::filebuf in_buf;
    if (!in_buf.open(png_filename.c_str(), std::ios::in | std::ios::binary))
    {
        BOOST_TEST(false);
    }
    std::istream in(&in_buf);

    gil::rgba8_image_t img;
    gil::read_image(in, img, gil::png_tag());
}

void test_subimage()
{
    run_subimage_test<gil::rgba8_image_t, gil::png_tag>(
        png_filename, gil::point_t(0, 0), gil::point_t(50, 50));

    run_subimage_test<gil::rgba8_image_t, gil::png_tag>(
        png_filename, gil::point_t(135, 95), gil::point_t(50, 50));
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

    gil::read_image(png_filename.c_str(), image, gil::png_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(png_out + "dynamic_image_test.png", gil::view(image), gil::png_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

int main()
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

    return boost::report_errors();
}

#else
int main() {}
#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
