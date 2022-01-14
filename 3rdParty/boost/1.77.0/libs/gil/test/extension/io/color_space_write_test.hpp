//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_TEST_EXTENSION_IO_COLOR_SPACE_WRITE_TEST_HPP
#define BOOST_GIL_TEST_EXTENSION_IO_COLOR_SPACE_WRITE_TEST_HPP

#include <boost/gil.hpp>

#ifndef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
#include <boost/core/ignore_unused.hpp>
#endif
#include <string>

#include "cmp_view.hpp"

template <typename Tag>
void color_space_write_test(std::string const& file_name_1, std::string const& file_name_2)
{
    namespace gil = boost::gil;

    gil::rgb8_image_t rgb(320, 200);
    gil::bgr8_image_t bgr(320, 200);

    gil::fill_pixels(gil::view(rgb), gil::rgb8_pixel_t(0, 0, 255));
    gil::fill_pixels(gil::view(bgr), gil::bgr8_pixel_t(255, 0, 0));

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::write_view(file_name_1, gil::view(rgb), Tag());
    gil::write_view(file_name_2, gil::view(bgr), Tag());
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES

#ifdef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    gil::rgb8_image_t rgb_1;
    gil::rgb8_image_t rgb_2;

    gil::read_image(file_name_1, rgb_1, Tag());
    gil::read_image(file_name_2, rgb_2, Tag());

    cmp_view(gil::view(rgb_1), gil::view(rgb_2));
#endif  // BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES

#ifndef BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES
    boost::ignore_unused(file_name_1);
    boost::ignore_unused(file_name_2);
#endif
}

#endif // BOOST_GIL_TEST_EXTENSION_IO_COLOR_SPACE_WRITE_TEST_HPP
