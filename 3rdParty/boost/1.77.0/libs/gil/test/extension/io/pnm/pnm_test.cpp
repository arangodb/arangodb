//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/io/pnm.hpp>

#include <boost/mp11.hpp>
#include <boost/core/lightweight_test.hpp>

#include <fstream>
#include <sstream>
#include <string>

#include "mandel_view.hpp"
#include "paths.hpp"
#include "subimage_test.hpp"

namespace gil = boost::gil;
namespace mp11 = boost::mp11;

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
void test_read_image_info_using_string()
{
    {
        using backend_t   = gil::get_reader_backend<std::string const, gil::pnm_tag>::type;
        backend_t backend = gil::read_image_info(pnm_filename, gil::pnm_tag());

        BOOST_TEST_EQ(backend._info._width, 256u);
        BOOST_TEST_EQ(backend._info._height, 256u);
    }
    {
        std::ifstream in(pnm_filename.c_str(), std::ios::binary);

        using backend_t   = gil::get_reader_backend<std::ifstream, gil::pnm_tag>::type;
        backend_t backend = gil::read_image_info(in, gil::pnm_tag());

        BOOST_TEST_EQ(backend._info._width, 256u);
        BOOST_TEST_EQ(backend._info._height, 256u);
    }
    {
        FILE* file = fopen(pnm_filename.c_str(), "rb");

        using backend_t   = gil::get_reader_backend<FILE*, gil::pnm_tag>::type;
        backend_t backend = gil::read_image_info(file, gil::pnm_tag());

        BOOST_TEST_EQ(backend._info._width, 256u);
        BOOST_TEST_EQ(backend._info._height, 256u);
    }
}

void test_read_image()
{
    {
        gil::rgb8_image_t img;
        gil::read_image(pnm_filename, img, gil::pnm_tag());

        BOOST_TEST_EQ(img.width(), 256u);
        BOOST_TEST_EQ(img.height(), 256u);
    }
    {
        std::ifstream in(pnm_filename.c_str(), std::ios::binary);

        gil::rgb8_image_t img;
        gil::read_image(in, img, gil::pnm_tag());

        BOOST_TEST_EQ(img.width(), 256u);
        BOOST_TEST_EQ(img.height(), 256u);
    }
    {
        FILE* file = fopen(pnm_filename.c_str(), "rb");

        gil::rgb8_image_t img;
        gil::read_image(file, img, gil::pnm_tag());

        BOOST_TEST_EQ(img.width(), 256u);
        BOOST_TEST_EQ(img.height(), 256u);
    }
}

void test_read_and_convert_image()
{
    {
        gil::rgb8_image_t img;
        gil::read_and_convert_image(pnm_filename, img, gil::pnm_tag());

        BOOST_TEST_EQ(img.width(), 256u);
        BOOST_TEST_EQ(img.height(), 256u);
    }
    {
        std::ifstream in(pnm_filename.c_str(), std::ios::binary);

        gil::rgb8_image_t img;
        gil::read_and_convert_image(in, img, gil::pnm_tag());

        BOOST_TEST_EQ(img.width(), 256u);
        BOOST_TEST_EQ(img.height(), 256u);
    }
    {
        FILE* file = fopen(pnm_filename.c_str(), "rb");

        gil::rgb8_image_t img;
        gil::read_and_convert_image(file, img, gil::pnm_tag());

        BOOST_TEST_EQ(img.width(), 256u);
        BOOST_TEST_EQ(img.height(), 256u);
    }
}

void test_read_view()
{
    {
        gil::rgb8_image_t img(256, 256);
        gil::read_view(pnm_filename, gil::view(img), gil::pnm_tag());
    }
    {
        std::ifstream in(pnm_filename.c_str(), std::ios::binary);

        gil::rgb8_image_t img(256, 256);
        gil::read_view(in, gil::view(img), gil::pnm_tag());
    }
    {
        FILE* file = fopen(pnm_filename.c_str(), "rb");

        gil::rgb8_image_t img(256, 256);
        gil::read_view(file, gil::view(img), gil::pnm_tag());
    }
}

void test_read_and_convert_view()
{
    {
        gil::rgb8_image_t img(256, 256);
        gil::read_and_convert_view(pnm_filename, gil::view(img), gil::pnm_tag());
    }
    {
        std::ifstream in(pnm_filename.c_str(), std::ios::binary);

        gil::rgb8_image_t img(256, 256);
        gil::read_and_convert_view(in, gil::view(img), gil::pnm_tag());
    }
    {
        FILE* file = fopen(pnm_filename.c_str(), "rb");

        gil::rgb8_image_t img(256, 256);

        gil::read_and_convert_view(file, gil::view(img), gil::pnm_tag());
    }
}

void test_stream()
{
    // 1. Read an image.
    std::ifstream in(pnm_filename.c_str(), std::ios::binary);

    gil::rgb8_image_t img;
    gil::read_image(in, img, gil::pnm_tag());

    // 2. Write image to in-memory buffer.
    std::stringstream out_buffer(std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    gil::write_view(out_buffer, gil::view(img), gil::pnm_tag());

    // 3. Copy in-memory buffer to another.
    std::stringstream in_buffer(std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    in_buffer << out_buffer.rdbuf();

    // 4. Read in-memory buffer to gil image
    gil::rgb8_image_t dst;
    gil::read_image(in_buffer, dst, gil::pnm_tag());

    // 5. Write out image.
    std::string filename(pnm_out + "stream_test.pnm");
    std::ofstream out(filename.c_str(), std::ios_base::binary);
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(out, gil::view(dst), gil::pnm_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

void test_stream_2()
{
    std::filebuf in_buf;
    if (!in_buf.open(pnm_filename.c_str(), std::ios::in | std::ios::binary))
    {
        BOOST_TEST(false);
    }

    std::istream in(&in_buf);

    gil::rgb8_image_t img;
    gil::read_image(in, img, gil::pnm_tag());
}

void test_subimage()
{
    run_subimage_test<gil::rgb8_image_t, gil::pnm_tag>(
        pnm_filename, gil::point_t(0, 0), gil::point_t(50, 50));

    run_subimage_test<gil::rgb8_image_t, gil::pnm_tag>(
        pnm_filename, gil::point_t(103, 103), gil::point_t(50, 50));
}

void test_dynamic_image_test()
{
    using my_img_types = mp11::mp_list
    <
        gil::gray8_image_t,
        gil::gray16_image_t,
        gil::rgb8_image_t,
        gil::gray1_image_t
    >;

    gil::any_image<my_img_types> image;

    gil::read_image(pnm_filename.c_str(), image, gil::pnm_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(pnm_out + "dynamic_image_test.pnm", gil::view(image), gil::pnm_tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
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
    test_dynamic_image_test();

    return boost::report_errors();
}
#else
int main() {}
#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
