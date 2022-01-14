//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#define BOOST_FILESYSTEM_VERSION 3
#define BOOST_GIL_IO_ADD_FS_PATH_SUPPORT
#include <boost/gil.hpp>
#include <boost/gil/extension/io/tiff.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/mp11.hpp>

#include <fstream>
#include <sstream>
#include <string>

#include "mandel_view.hpp"
#include "paths.hpp"
#include "subimage_test.hpp"

namespace fs   = boost::filesystem;
namespace gil  = boost::gil;
namespace mp11 = boost::mp11;

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

void test_read_image_info()
{
    {
        using backend_t   = gil::get_reader_backend<std::string const, gil::tiff_tag>::type;
        backend_t backend = gil::read_image_info(tiff_filename, gil::tiff_tag());

        BOOST_TEST_EQ(backend._info._width, 1000u);
        BOOST_TEST_EQ(backend._info._height, 600u);
    }
    {
        std::ifstream in(tiff_filename.c_str(), std::ios::binary);

        using backend_t   = gil::get_reader_backend<std::ifstream, gil::tiff_tag>::type;
        backend_t backend = gil::read_image_info(in, gil::tiff_tag());

        BOOST_TEST_EQ(backend._info._width, 1000u);
        BOOST_TEST_EQ(backend._info._height, 600u);
    }
    {
        TIFF* file = TIFFOpen(tiff_filename.c_str(), "r");

        using backend_t   = gil::get_reader_backend<FILE*, gil::tiff_tag>::type;
        backend_t backend = gil::read_image_info(file, gil::tiff_tag());

        BOOST_TEST_EQ(backend._info._width, 1000u);
        BOOST_TEST_EQ(backend._info._height, 600u);
    }
    {
        fs::path my_path(tiff_filename);

        using backend_t   = gil::get_reader_backend<fs::path, gil::tiff_tag>::type;
        backend_t backend = gil::read_image_info(my_path, gil::tiff_tag());

        BOOST_TEST_EQ(backend._info._width, 1000u);
        BOOST_TEST_EQ(backend._info._height, 600u);
    }
}

void test_read_image()
{
    {
        gil::rgba8_image_t img;
        gil::read_image(tiff_filename, img, gil::tiff_tag());

        BOOST_TEST_EQ(img.width(), 1000u);
        BOOST_TEST_EQ(img.height(), 600u);
    }
    {
        std::ifstream in(tiff_filename.c_str(), std::ios::binary);

        gil::rgba8_image_t img;
        gil::read_image(in, img, gil::tiff_tag());

        BOOST_TEST_EQ(img.width(), 1000u);
        BOOST_TEST_EQ(img.height(), 600u);
    }
    {
        TIFF* file = TIFFOpen(tiff_filename.c_str(), "r");

        gil::rgba8_image_t img;
        gil::read_image(file, img, gil::tiff_tag());

        BOOST_TEST_EQ(img.width(), 1000u);
        BOOST_TEST_EQ(img.height(), 600u);
    }
}

void test_read_and_convert_image()
{
    {
        gil::rgb8_image_t img;
        gil::read_and_convert_image(tiff_filename, img, gil::tiff_tag());

        BOOST_TEST_EQ(img.width(), 1000u);
        BOOST_TEST_EQ(img.height(), 600u);
    }
    {
        std::ifstream in(tiff_filename.c_str(), std::ios::binary);

        gil::rgb8_image_t img;
        gil::read_and_convert_image(in, img, gil::tiff_tag());

        BOOST_TEST_EQ(img.width(), 1000u);
        BOOST_TEST_EQ(img.height(), 600u);
    }
    {
        TIFF* file = TIFFOpen(tiff_filename.c_str(), "r");

        gil::rgb8_image_t img;
        gil::read_and_convert_image(file, img, gil::tiff_tag());

        BOOST_TEST_EQ(img.width(), 1000u);
        BOOST_TEST_EQ(img.height(), 600u);
    }
}

void test_read_and_convert_image_2()
{
    gil::gray8_image_t img;
    gil::read_and_convert_image(tiff_filename, img, gil::tiff_tag());

    gil::rgba8_image_t img2;
    gil::read_image(tiff_filename, img2, gil::tiff_tag());

    BOOST_TEST(gil::equal_pixels(
        gil::const_view(img),
        gil::color_converted_view<gil::gray8_pixel_t>(gil::const_view(img2))));
}

void test_read_view()
{
    {
        gil::rgba8_image_t img(1000, 600);
        gil::read_view(tiff_filename, gil::view(img), gil::tiff_tag());
    }
    {
        std::ifstream in(tiff_filename.c_str(), std::ios::binary);

        gil::rgba8_image_t img(1000, 600);
        gil::read_view(in, gil::view(img), gil::tiff_tag());

        BOOST_TEST_EQ(img.width(), 1000u);
        BOOST_TEST_EQ(img.height(), 600u);
    }
    {
        TIFF* file = TIFFOpen(tiff_filename.c_str(), "r");

        gil::rgba8_image_t img(1000, 600);
        gil::read_view(file, gil::view(img), gil::tiff_tag());
    }
}

void test_read_and_convert_view()
{
    {
        gil::rgb8_image_t img(1000, 600);
        gil::read_and_convert_view(tiff_filename, gil::view(img), gil::tiff_tag());
    }
    {
        std::ifstream in(tiff_filename.c_str(), std::ios::binary);

        gil::rgb8_image_t img(1000, 600);
        gil::read_and_convert_view(in, gil::view(img), gil::tiff_tag());

        BOOST_TEST_EQ(img.width(), 1000u);
        BOOST_TEST_EQ(img.height(), 600u);
    }
    {
        TIFF* file = TIFFOpen(tiff_filename.c_str(), "r");

        gil::rgb8_image_t img(1000, 600);
        gil::read_and_convert_view(file, gil::view(img), gil::tiff_tag());
    }
}

void test_write_view()
{
    auto const b = gil::rgb8_pixel_t(0, 0, 255);
    auto const g = gil::rgb8_pixel_t(0, 255, 0);
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    {
        std::string filename(tiff_out + "write_test_string.tif");

        gil::write_view(filename, create_mandel_view(320, 240, b, g), gil::tiff_tag());
    }
    {
        std::string filename(tiff_out + "write_test_ofstream.tif");
        std::ofstream out(filename.c_str(), std::ios_base::binary);

        gil::write_view(out, create_mandel_view(320, 240, b, g), gil::tiff_tag());
    }
    {
        std::string filename(tiff_out + "write_test_tiff.tif");
        TIFF* file = TIFFOpen(filename.c_str(), "w");

        gil::write_view(file, create_mandel_view(320, 240, b, g), gil::tiff_tag());
    }
    {
        std::string filename(tiff_out + "write_test_info.tif");

        gil::image_write_info<gil::tiff_tag> info;
        gil::write_view(filename, create_mandel_view(320, 240, b, g), info);
    }
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

void test_stream()
{
    // 1. Read an image.
    std::ifstream in(tiff_filename.c_str(), std::ios::binary);

    gil::rgba8_image_t img;
    gil::read_image(in, img, gil::tiff_tag());

    // 2. Write image to in-memory buffer.
    std::stringstream out_buffer(std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    gil::write_view(out_buffer, gil::view(img), gil::tiff_tag());

    // 3. Copy in-memory buffer to another.
    std::stringstream in_buffer(std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    in_buffer << out_buffer.rdbuf();

    // 4. Read in-memory buffer to gil image
    gil::rgba8_image_t dst;
    gil::read_image(in_buffer, dst, gil::tiff_tag());

    // 5. Write out image.
    std::string filename(tiff_out + "stream_test.tif");
    std::ofstream out(filename.c_str(), std::ios_base::binary);
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(out, gil::view(dst), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

void test_stream_2()
{
    std::filebuf in_buf;
    if (!in_buf.open(tiff_filename.c_str(), std::ios::in | std::ios::binary))
    {
        BOOST_TEST(false);
    }
    std::istream in(&in_buf);

    gil::rgba8_image_t img;
    gil::read_image(in, img, gil::tiff_tag());
}

void test_subimage()
{
    run_subimage_test<gil::rgba8_image_t, gil::tiff_tag>(
        tiff_filename, gil::point_t(0, 0), gil::point_t(50, 50));

    run_subimage_test<gil::rgba8_image_t, gil::tiff_tag>(
        tiff_filename, gil::point_t(50, 50), gil::point_t(50, 50));
}

void test_dynamic_image()
{
    // FIXME: This test has been disabled for now because of compilation issues with MSVC10.

    using my_img_types = mp11::mp_list
    <
        gil::gray8_image_t,
        gil::gray16_image_t,
        gil::rgb8_image_t,
        gil::rgba8_image_t,
        gil::gray1_image_t
    >;
    gil::any_image<my_img_types> image;

    gil::read_image(tiff_filename.c_str(), image, gil::tiff_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(tiff_out + "dynamic_image_test.tif", gil::view(image), gil::tiff_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

int main()
{
    test_read_image_info();
    test_read_image();
    test_read_and_convert_image();
    test_read_and_convert_image_2();
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
#endif  // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
