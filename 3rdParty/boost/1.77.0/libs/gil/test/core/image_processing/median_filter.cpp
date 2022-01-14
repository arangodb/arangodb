//
// Copyright 2019 Miral Shah <miralshah2211@gmail.com>
//
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/gil/image_view.hpp>
#include <boost/gil/algorithm.hpp>
#include <boost/gil/gray.hpp>
#include <boost/gil/image_processing/filter.hpp>

#include <boost/core/lightweight_test.hpp>

namespace gil = boost::gil;

std::uint8_t img[] =
{
    183, 128, 181, 86,  34,  55, 134, 164,  15,
    90,  59,  94, 158, 202,   0, 106, 120, 255,
    65,  48,   4,  21,  21,  38,  50,  37, 228,
    27, 245, 254, 164, 135, 192,  17, 241,  19,
    56, 165, 253, 169,  24, 200, 249,  70, 199,
    59,  84,  41,  96,  70,  58,  24,  20, 218,
    235, 180,  12, 168, 224, 204, 166, 153,  1,
    181, 213, 232, 178, 165, 253,  93, 214, 72,
    171,  50,  20,  65,  67, 133, 249, 157, 105
};

std::uint8_t output[] =
{
    128, 128, 128, 94,  55,  55, 120, 134, 120,
    90,  90,  86,  86,  38,  50,  55, 120, 164,
    65,  65,  94, 135, 135,  50,  50, 106, 228,
    56,  65, 165, 135, 135,  50,  70,  70, 199,
    59,  84, 165, 135, 135,  70,  70,  70, 199,
    84,  84, 165,  96, 168, 166, 153, 153, 153,
    181, 180, 168, 165, 168, 165, 153,  93, 72,
    181, 180, 168, 165, 168, 166, 166, 153, 105,
    171, 171,  65,  67, 133, 133, 157, 157, 105
};

void test_median_filter_with_kernel_size_3()
{
    gil::gray8c_view_t src_view =
        gil::interleaved_view(9, 9, reinterpret_cast<const gil::gray8_pixel_t*>(img), 9);

    gil::image<gil::gray8_pixel_t> temp_img(src_view.width(), src_view.height());
    typename gil::image<gil::gray8_pixel_t>::view_t temp_view = view(temp_img);
    gil::gray8_view_t dst_view(temp_view);

    gil::median_filter(src_view, dst_view, 3);

    gil::gray8c_view_t out_view =
        gil::interleaved_view(9, 9, reinterpret_cast<const gil::gray8_pixel_t*>(output), 9);

    BOOST_TEST(gil::equal_pixels(out_view, dst_view));
}

int main()
{
    test_median_filter_with_kernel_size_3();

    return ::boost::report_errors();
}
