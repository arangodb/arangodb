//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/io/targa/old.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/mp11.hpp>

#include "mandel_view.hpp"
#include "paths.hpp"

namespace gil  = boost::gil;
namespace mp11 = boost::mp11;

void test_old_read_dimensions()
{
    boost::gil::point_t dim = gil::targa_read_dimensions(targa_filename);

    BOOST_TEST_EQ(dim.x, 124);
    BOOST_TEST_EQ(dim.y, 124);
}

void test_old_read_image()
{
    gil::rgb8_image_t img;
    gil::targa_read_image(targa_filename, img);

    BOOST_TEST_EQ(img.width(), 124);
    BOOST_TEST_EQ(img.height(), 124);
}

void test_old_read_and_convert_image()
{
    gil::rgb8_image_t img;
    gil::targa_read_and_convert_image(targa_filename, img);

    BOOST_TEST_EQ(img.width(), 124);
    BOOST_TEST_EQ(img.height(), 124);
}

void test_old_read_view()
{
    gil::rgb8_image_t img(124, 124);
    gil::targa_read_view(targa_filename, gil::view(img));
}

void test_old_read_and_convert_view()
{
    gil::rgb8_image_t img(124, 124);
    gil::targa_read_and_convert_view(targa_filename, gil::view(img));
}

void test_old_write_view()
{
    targa_write_view(
        targa_out + "old_write_view_test.tga",
        create_mandel_view(124, 124, gil::rgb8_pixel_t(0, 0, 255), gil::rgb8_pixel_t(0, 255, 0)));
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
    gil::targa_read_image(targa_filename.c_str(), image);

    targa_write_view(targa_out + "old_dynamic_image_test.tga", gil::view(image));
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
