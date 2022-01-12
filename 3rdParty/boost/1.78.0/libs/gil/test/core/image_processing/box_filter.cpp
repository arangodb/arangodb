//
// Copyright 2019 Miral Shah <miralshah2211@gmail.com>
//
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/gil/algorithm.hpp>
#include <boost/gil/gray.hpp>
#include <boost/gil/image_view.hpp>
#include <boost/gil/image_processing/filter.hpp>

#include <boost/core/lightweight_test.hpp>

namespace gil = boost::gil;

std::uint8_t img[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 255, 0, 0, 0, 255, 0, 0,
    0, 0, 0, 255, 0, 255, 0, 0, 0,
    0, 0, 0, 0, 255, 0, 0, 0, 0,
    0, 0, 0, 255, 0, 255, 0, 0, 0,
    0, 0, 255, 0, 0, 0, 255, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0
};

std::uint8_t output[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 28, 28, 28, 0, 28, 28, 28, 0,
    0, 28, 56, 56, 56, 56, 56, 28, 0,
    0, 28, 56, 85, 85, 85, 56, 28, 0,
    0, 0, 56, 85, 141, 85, 56, 0, 0,
    0, 28, 56, 85, 85, 85, 56, 28, 0,
    0, 28, 56, 56, 56, 56, 56, 28, 0,
    0, 28, 28, 28, 0, 28, 28, 28, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0
};

void test_box_filter_with_default_parameters()
{
    gil::gray8c_view_t src_view =
        gil::interleaved_view(9, 9, reinterpret_cast<const gil::gray8_pixel_t*>(img), 9);

    gil::image<gil::gray8_pixel_t> temp_img(src_view.width(), src_view.height());
    typename gil::image<gil::gray8_pixel_t>::view_t temp_view = view(temp_img);
    gil::gray8_view_t dst_view(temp_view);

    gil::box_filter(src_view, dst_view, 3);

    gil::gray8c_view_t out_view =
        gil::interleaved_view(9, 9, reinterpret_cast<const gil::gray8_pixel_t*>(output), 9);

    BOOST_TEST(gil::equal_pixels(out_view, dst_view));
}

int main()
{
    test_box_filter_with_default_parameters();

    return ::boost::report_errors();
}
