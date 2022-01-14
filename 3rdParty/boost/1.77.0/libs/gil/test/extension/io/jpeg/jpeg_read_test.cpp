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

#include "paths.hpp"
#include "scanline_read_test.hpp"

#include <cmath>
#include <sstream>
#include <string>

namespace gil = boost::gil;

#ifdef BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES

void test_read_header()
{
    using backend_t = gil::get_reader_backend<std::string const, gil::jpeg_tag>::type;
    backend_t backend = gil::read_image_info(jpeg_filename, gil::jpeg_tag());

    BOOST_TEST_EQ(backend._info._width, 1000u);
    BOOST_TEST_EQ(backend._info._height, 600u);

    BOOST_TEST_EQ(backend._info._num_components, 3);
    BOOST_TEST_EQ(backend._info._color_space, JCS_YCbCr);

    BOOST_TEST_EQ(backend._info._data_precision, 8);
}

void test_read_pixel_density()
{
    using backend_t = gil::get_reader_backend<std::string const, gil::jpeg_tag>::type;
    backend_t backend = gil::read_image_info(jpeg_in + "EddDawson/36dpi.jpg", gil::jpeg_tag());

    gil::rgb8_image_t img;
    gil::read_image(jpeg_in + "EddDawson/36dpi.jpg", img, gil::jpeg_tag());

    gil::image_write_info<gil::jpeg_tag> write_settings;
    write_settings.set_pixel_dimensions(backend._info._width, backend._info._height, backend._info._pixel_width_mm, backend._info._pixel_height_mm);

    std::stringstream in_memory(std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    gil::write_view(in_memory, gil::view(img), write_settings);

    using backend2_t = gil::get_reader_backend<std::stringstream, gil::jpeg_tag>::type;
    backend2_t backend2 = gil::read_image_info(in_memory, gil::jpeg_tag());

    // Because of rounding the two results differ slightly.
    if (std::abs(backend._info._pixel_width_mm - backend2._info._pixel_width_mm) > 10.0 || std::abs(backend._info._pixel_height_mm - backend2._info._pixel_height_mm) > 10.0)
    {
        BOOST_TEST_EQ(0, 1);
    }
}

void test_read_reference_images()
{
    using image_t = gil::rgb8_image_t;
    image_t img;
    read_image(jpeg_filename, img, gil::jpeg_tag());

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(jpeg_out + "rgb8_test.jpg", gil::view(img), gil::jpeg_tag());
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

void test_dct_method_read()
{
    using image_t = gil::rgb8_image_t;
    image_t img;

    gil::image_read_settings<gil::jpeg_tag> settings;
    settings._dct_method = gil::jpeg_dct_method::fast;

    gil::read_image(jpeg_filename, img, settings);

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(jpeg_out + "fast_dct_read_test.jpg", gil::view(img), gil::jpeg_tag());
#endif // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
}

void test_read_reference_images_image_iterator()
{
    test_scanline_reader<gil::rgb8_image_t, gil::jpeg_tag>(jpeg_filename.c_str());
}

int main()
{
    test_read_header();
    test_read_pixel_density();
    test_read_reference_images();
    test_dct_method_read();
    test_read_reference_images_image_iterator();

    return boost::report_errors();
}

#else
int main() {}
#endif // BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES
