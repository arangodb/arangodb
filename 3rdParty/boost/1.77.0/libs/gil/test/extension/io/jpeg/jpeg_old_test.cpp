//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/io/jpeg/old.hpp>

#include <boost/mp11.hpp>
#include <boost/core/lightweight_test.hpp>

#include "mandel_view.hpp"
#include "paths.hpp"

namespace gil = boost::gil;
namespace mp11 = boost::mp11;

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

void test_old_read_dimensions()
{
    gil::point_t dim = gil::jpeg_read_dimensions(jpeg_filename);
    BOOST_TEST_EQ(dim.x, 1000);
    BOOST_TEST_EQ(dim.y, 600);
}

void test_old_read_image()
{
    gil::rgb8_image_t img;
    gil::jpeg_read_image(jpeg_filename, img);

    BOOST_TEST_EQ(img.width(), 1000);
    BOOST_TEST_EQ(img.height(), 600);
}

void test_old_read_and_convert_image()
{
    gil::rgb8_image_t img;
    gil::jpeg_read_and_convert_image(jpeg_filename, img);

    BOOST_TEST_EQ(img.width(), 1000);
    BOOST_TEST_EQ(img.height(), 600);
}

void test_old_read_view()
{
    gil::rgb8_image_t img(1000, 600);
    gil::jpeg_read_view(jpeg_filename, gil::view(img));
}

void test_old_read_and_convert_view()
{
    gil::rgb8_image_t img(1000, 600);
    gil::jpeg_read_and_convert_view(jpeg_filename, gil::view(img));
}

void test_old_write_view()
{
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    auto b = gil::rgb8_pixel_t(0, 0, 255);
    auto g = gil::rgb8_pixel_t(0, 255, 0);
    gil::jpeg_write_view(jpeg_out + "old_write_test.jpg", create_mandel_view(320, 240, b, g));
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

void test_old_dynamic_image()
{
    using my_img_types = mp11::mp_list
        <
            gil::gray8_image_t,
            gil::gray16_image_t,
            gil::rgb8_image_t,
            gil::rgba8_image_t
        >;

    gil::any_image<my_img_types> image;
    gil::jpeg_read_image(jpeg_filename.c_str(), image);

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::jpeg_write_view(jpeg_out + "old_dynamic_image_test.jpg", gil::view(image));
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

int main()
{
    test_old_read_dimensions();
    test_old_read_image();
    test_old_read_and_convert_image();
    test_old_read_view();
    test_old_read_and_convert_view();
    test_old_write_view();
    test_old_dynamic_image();

    return boost::report_errors();
}

#else
int main() {}
#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
