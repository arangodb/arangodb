//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/io/pnm/old.hpp>

#include <boost/mp11.hpp>
#include <boost/core/lightweight_test.hpp>

#include "paths.hpp"

namespace gil  = boost::gil;
namespace mp11 = boost::mp11;

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
void test_old_read_dimensions()
{
    boost::gil::point_t dim = gil::pnm_read_dimensions(pnm_filename);
    BOOST_TEST_EQ(dim.x, 256);
    BOOST_TEST_EQ(dim.y, 256);
}

void test_old_read_image()
{
    gil::rgb8_image_t img;
    gil::pnm_read_image(pnm_filename, img);

    BOOST_TEST_EQ(img.width(), 256);
    BOOST_TEST_EQ(img.height(), 256);
}

void test_old_read_and_convert_image()
{
    gil::rgb8_image_t img;
    gil::pnm_read_and_convert_image(pnm_filename, img);

    BOOST_TEST_EQ(img.width(), 256);
    BOOST_TEST_EQ(img.height(), 256);
}

void test_old_read_view()
{
    gil::rgb8_image_t img(256, 256);
    gil::pnm_read_view(pnm_filename, gil::view(img));
}

void test_old_read_and_convert_view()
{
    gil::rgb8_image_t img(256, 256);
    gil::pnm_read_and_convert_view(pnm_filename, gil::view(img));
}

void test_old_write_view()
{
    std::string filename(pnm_out + "test5.pnm");
    gil::gray8_image_t img(256, 256);

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::pnm_write_view(filename, gil::view(img));
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

void test_old_dynamic_image()
{
    using my_img_types =
        mp11::mp_list<gil::gray8_image_t, gil::gray16_image_t, gil::rgb8_image_t, gil::gray1_image_t>;

    gil::any_image<my_img_types> image;

    gil::pnm_read_image(pnm_filename.c_str(), image);

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::pnm_write_view(pnm_out + "old_dynamic_image_test.pnm", gil::view(image));
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
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
#endif  // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
