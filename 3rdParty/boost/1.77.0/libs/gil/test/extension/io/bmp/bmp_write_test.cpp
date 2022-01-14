//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/io/typedefs.hpp>
#include <boost/gil/extension/io/bmp.hpp>

#include <boost/core/lightweight_test.hpp>

#include "cmp_view.hpp"
#include "color_space_write_test.hpp"
#include "mandel_view.hpp"
#include "paths.hpp"

namespace gil = boost::gil;

void test_write_rgb8()
{
    gil::write_view(bmp_out + "rgb8_test.bmp", create_mandel_view(200, 200,
        gil::rgb8_pixel_t(0, 0, 255), gil::rgb8_pixel_t(0, 255, 0)), gil::bmp_tag());
}

void test_write_rgba8()
{
    gil::write_view(bmp_out + "rgba8_test.bmp", create_mandel_view(200, 200,
        gil::rgba8_pixel_t(0, 0, 255, 0), gil::rgba8_pixel_t(0, 255, 0, 0)), gil::bmp_tag());
}

void test_rgb_color_space_write()
{
    color_space_write_test<gil::bmp_tag>(
        bmp_out + "rgb_color_space_test.bmp",
        bmp_out + "bgr_color_space_test.bmp");
}

int main(int argc, char *argv[])
{
    try
    {
        test_write_rgb8();
        test_write_rgba8();
        test_rgb_color_space_write();
    }
    catch (std::exception const& e)
    {
        BOOST_ERROR(e.what());
    }
    return boost::report_errors();
}
