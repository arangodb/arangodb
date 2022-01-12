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
#include <boost/gil/extension/io/jpeg.hpp>

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

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

void test_read_image_info()
{
    {
        using backend_t = gil::get_reader_backend<std::string const, gil::jpeg_tag>::type;
        backend_t backend = gil::read_image_info(jpeg_filename, gil::jpeg_tag());

        BOOST_TEST_EQ(backend._info._width, 1000u);
        BOOST_TEST_EQ(backend._info._height, 600u);
    }
    {
        std::ifstream in(jpeg_filename.c_str(), std::ios::binary);

        using backend_t = gil::get_reader_backend<std::ifstream, gil::jpeg_tag>::type;
        backend_t backend = gil::read_image_info(in, gil::jpeg_tag());

        BOOST_TEST_EQ(backend._info._width, 1000u);
        BOOST_TEST_EQ(backend._info._height, 600u);
    }
    {
        FILE *file = fopen(jpeg_filename.c_str(), "rb");

        using backend_t = gil::get_reader_backend<FILE *, gil::jpeg_tag>::type;
        backend_t backend = gil::read_image_info(file, gil::jpeg_tag());

        BOOST_TEST_EQ(backend._info._width, 1000u);
        BOOST_TEST_EQ(backend._info._height, 600u);
    }
    {
        using backend_t = gil::get_reader_backend<fs::path, gil::jpeg_tag>::type;
        backend_t backend = gil::read_image_info(fs::path(jpeg_filename), gil::jpeg_tag());

        BOOST_TEST_EQ(backend._info._width, 1000u);
        BOOST_TEST_EQ(backend._info._height, 600u);
    }
}

void test_read_image()
{
    {
        gil::rgb8_image_t img;
        gil::read_image(jpeg_filename, img, gil::jpeg_tag());

        BOOST_TEST_EQ(img.width(), 1000u);
        BOOST_TEST_EQ(img.height(), 600u);
    }
    {
        std::ifstream in(jpeg_filename.c_str(), std::ios::binary);
        gil::rgb8_image_t img;
        gil::read_image(in, img, gil::jpeg_tag());

        BOOST_TEST_EQ(img.width(), 1000u);
        BOOST_TEST_EQ(img.height(), 600u);
    }
    {
        FILE *file = fopen(jpeg_filename.c_str(), "rb");
        gil::rgb8_image_t img;
        gil::read_image(file, img, gil::jpeg_tag());

        BOOST_TEST_EQ(img.width(), 1000u);
        BOOST_TEST_EQ(img.height(), 600u);
    }
    {
        gil::rgb8_image_t img;
        gil::image_read_settings<gil::jpeg_tag> settings(gil::point_t(0, 0), gil::point_t(10, 10), gil::jpeg_dct_method::slow);
        gil::read_image(jpeg_filename, img, settings);

        BOOST_TEST_EQ(img.width(), 10u);
        BOOST_TEST_EQ(img.height(), 10u);
    }
}

void test_read_and_convert_image()
{
    {
        gil::rgb8_image_t img;
        gil::read_and_convert_image(jpeg_filename, img, gil::jpeg_tag());
    }
    {
        std::ifstream in(jpeg_filename.c_str(), std::ios::binary);

        gil::rgb8_image_t img;
        gil::read_and_convert_image(in, img, gil::jpeg_tag());
    }
}

void test_read_view()
{
    {
        gil::rgb8_image_t img(1000, 600);
        gil::read_view(jpeg_filename, gil::view(img), gil::jpeg_tag());
    }
    {
        std::ifstream in(jpeg_filename.c_str(), std::ios::binary);

        gil::rgb8_image_t img(1000, 600);
        gil::read_view(in, gil::view(img), gil::jpeg_tag());
    }
    {
        FILE *file = fopen(jpeg_filename.c_str(), "rb");

        gil::rgb8_image_t img(1000, 600);
        gil::read_view(file, gil::view(img), gil::jpeg_tag());
    }
}

void test_read_and_convert_view()
{
    {
        gil::rgb8_image_t img(1000, 600);
        gil::read_and_convert_view(jpeg_filename, gil::view(img), gil::jpeg_tag());
    }
    {
        std::ifstream in(jpeg_filename.c_str(), std::ios::binary);

        gil::rgb8_image_t img(1000, 600);
        gil::read_and_convert_view(in, gil::view(img), gil::jpeg_tag());
    }
    {
        FILE *file = fopen(jpeg_filename.c_str(), "rb");

        gil::rgb8_image_t img(1000, 600);
        gil::read_and_convert_view(file, gil::view(img), gil::jpeg_tag());
    }
}

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
void test_write_view()
{
    auto const b = gil::rgb8_pixel_t(0, 0, 255);
    auto const g = gil::rgb8_pixel_t(0, 255, 0);
    {
        std::string filename(jpeg_out + "write_test_string.jpg");

        gil::write_view(filename, create_mandel_view(320, 240, b, g), gil::jpeg_tag());
    }
    {
        std::string filename(jpeg_out + "write_test_ofstream.jpg");
        std::ofstream out(filename.c_str(), std::ios::binary);

        gil::write_view(out, create_mandel_view(320, 240, b, g), gil::jpeg_tag());
    }
    {
        std::string filename(jpeg_out + "write_test_file.jpg");
        FILE *file = fopen(filename.c_str(), "wb");

        gil::write_view(file, create_mandel_view(320, 240, b, g), gil::jpeg_tag());
    }
    {
        std::string filename(jpeg_out + "write_test_info.jpg");
        FILE *file = fopen(filename.c_str(), "wb");

        gil::image_write_info<gil::jpeg_tag> info;
        gil::write_view(file, create_mandel_view(320, 240, b, g), info);
    }
}
#endif //BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES

void test_stream()
{
    // 1. Read an image.
    std::ifstream in(jpeg_filename.c_str(), std::ios::binary);

    gil::rgb8_image_t img;
    gil::read_image(in, img, gil::jpeg_tag());

    // 2. Write image to in-memory buffer.
    std::stringstream out_buffer(std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    gil::write_view(out_buffer, gil::view(img), gil::jpeg_tag());

    // 3. Copy in-memory buffer to another.
    std::stringstream in_buffer(std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    in_buffer << out_buffer.rdbuf();

    // 4. Read in-memory buffer to gil image
    gil::rgb8_image_t dst;
    gil::read_image(in_buffer, dst, gil::jpeg_tag());

    // 5. Write out image.
    std::string filename(jpeg_out + "stream_test.jpg");
    std::ofstream out(filename.c_str(), std::ios_base::binary);
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(out, gil::view(dst), gil::jpeg_tag());
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

void test_stream_2()
{
    std::filebuf in_buf;
    if (!in_buf.open(jpeg_filename.c_str(), std::ios::in | std::ios::binary))
    {
        BOOST_TEST(false);
    }

    std::istream in(&in_buf);

    gil::rgb8_image_t img;
    gil::read_image(in, img, gil::jpeg_tag());
}

void test_subimage()
{
    run_subimage_test<gil::rgb8_image_t, gil::jpeg_tag>(jpeg_filename,
        gil::point_t(0, 0), gil::point_t(50, 50));

    run_subimage_test<gil::rgb8_image_t, gil::jpeg_tag>(jpeg_filename,
        gil::point_t(43, 24), gil::point_t(50, 50));
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
    gil::read_image(jpeg_filename.c_str(), image, gil::jpeg_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(jpeg_out + "old_dynamic_image_test.jpg", gil::view(image), gil::jpeg_tag());
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

int main()
{
    test_read_image_info();
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
