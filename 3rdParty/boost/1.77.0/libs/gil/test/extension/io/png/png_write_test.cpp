//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#define BOOST_GIL_IO_ADD_FS_PATH_SUPPORT
#define BOOST_GIL_IO_ENABLE_GRAY_ALPHA
#define BOOST_FILESYSTEM_VERSION 3
#include <boost/gil.hpp>
#include <boost/gil/extension/io/png.hpp>

#include <boost/core/lightweight_test.hpp>

#include "color_space_write_test.hpp"
#include "paths.hpp"
#include "scanline_read_test.hpp"

namespace gil = boost::gil;

void test_rgb_color_space_write()
{
    color_space_write_test<gil::png_tag>(
        png_out + "rgb_color_space_test.png", png_out + "bgr_color_space_test.png");
}

int main()
{
    test_rgb_color_space_write();

    return boost::report_errors();
}
