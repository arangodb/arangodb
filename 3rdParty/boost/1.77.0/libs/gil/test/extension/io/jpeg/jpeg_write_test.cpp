//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/io/jpeg.hpp>

#include <boost/core/lightweight_test.hpp>

#include "color_space_write_test.hpp"
#include "mandel_view.hpp"
#include "paths.hpp"

namespace gil = boost::gil;

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
void test_write_gray8()
{
    gil::write_view(jpeg_out + "gray8_test.jpg", create_mandel_view(200, 200,
        gil::gray8_pixel_t(0), gil::gray8_pixel_t(255)), gil::jpeg_tag());
}

void test_write_rgb8()
{
    gil::write_view(jpeg_out + "rgb8_test.jpg", create_mandel_view(200, 200,
        gil::rgb8_pixel_t(0, 0, 255), gil::rgb8_pixel_t(0, 255, 0)), gil::jpeg_tag());
}

void test_write_cmyk8()
{
    gil::write_view(jpeg_out + "cmyk8_test.jpg", create_mandel_view(200, 200,
        gil::cmyk8_pixel_t(0, 0, 255, 127), gil::cmyk8_pixel_t(0, 255, 0, 127)), gil::jpeg_tag());
}
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

void test_dct_method_write_test()
{
    using image_t = gil::rgb8_image_t;
    image_t img;

    gil::read_image(jpeg_filename, img, gil::jpeg_tag());
    gil::image_write_info<gil::jpeg_tag> info;
    info._dct_method = gil::jpeg_dct_method::fast;

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(jpeg_out + "fast_dct_write_test.jpg", gil::view(img), info);
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

void test_rgb_color_space_write_test()
{
    color_space_write_test<gil::jpeg_tag>(
        jpeg_out + "rgb_color_space_test.jpg",
        jpeg_out + "bgr_color_space_test.jpg");
}

int main()
{
#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    test_write_gray8();
    test_write_rgb8();
    test_write_cmyk8();
#endif

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
    test_dct_method_write_test();
#endif

    test_rgb_color_space_write_test();

    return boost::report_errors();
}


