//
// Copyright 2019 Pranam Lashkari <plashkari628@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/numeric/algorithm.hpp>
#include <boost/gil/extension/numeric/convolve.hpp>

#include <boost/core/lightweight_test.hpp>

namespace gil = boost::gil;

std::uint8_t img[] =
{
    1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 0, 0, 0, 0, 0, 0, 0, 32,
    11, 0, 255, 0, 0, 0, 255, 0, 31,
    12, 0, 0, 255, 0, 255, 0, 0, 30,
    13, 0, 0, 0, 255, 0, 0, 0, 29,
    14, 0, 0, 255, 0, 255, 0, 0, 28,
    15, 0, 255, 0, 0, 0, 255, 0, 27,
    16, 0, 0, 0, 0, 0, 0, 0, 26,
    17, 18, 19, 20, 21, 22, 23, 24, 25
};

std::uint8_t row_output_constant[] =
{
    1, 2, 3, 4, 5, 6, 7, 8, 9,
    1, 2, 3, 4, 5, 6, 7, 8, 9,
    1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 0, 0, 0, 0, 0, 0, 0, 32,
    11, 0, 255, 0, 0, 0, 255, 0, 31,
    12, 0, 0, 255, 0, 255, 0, 0, 30,
    13, 0, 0, 0, 255, 0, 0, 0, 29,
    14, 0, 0, 255, 0, 255, 0, 0, 28,
    15, 0, 255, 0, 0, 0, 255, 0, 27,
    16, 0, 0, 0, 0, 0, 0, 0, 26,
    17, 18, 19, 20, 21, 22, 23, 24, 25,
    17, 18, 19, 20, 21, 22, 23, 24, 25,
    17, 18, 19, 20, 21, 22, 23, 24, 25
};

std::uint8_t row_output_zero[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 0, 0, 0, 0, 0, 0, 0, 32,
    11, 0, 255, 0, 0, 0, 255, 0, 31,
    12, 0, 0, 255, 0, 255, 0, 0, 30,
    13, 0, 0, 0, 255, 0, 0, 0, 29,
    14, 0, 0, 255, 0, 255, 0, 0, 28,
    15, 0, 255, 0, 0, 0, 255, 0, 27,
    16, 0, 0, 0, 0, 0, 0, 0, 26,
    17, 18, 19, 20, 21, 22, 23, 24, 25,
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0,
};

std::uint8_t col_output_constant[] =
{
    1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9,
    10, 10, 0, 0, 0, 0, 0, 0, 0, 32, 32,
    11, 11, 0, 255, 0, 0, 0, 255, 0, 31, 31,
    12, 12, 0, 0, 255, 0, 255, 0, 0, 30, 30,
    13, 13, 0, 0, 0, 255, 0, 0, 0, 29, 29,
    14, 14, 0, 0, 255, 0, 255, 0, 0, 28, 28,
    15, 15, 0, 255, 0, 0, 0, 255, 0, 27, 27,
    16, 16, 0, 0, 0, 0, 0, 0, 0, 26, 26,
    17, 17, 18, 19, 20, 21, 22, 23, 24, 25, 25
};

std::uint8_t col_output_zero[] =
{
    0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0,
    0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0,
    0, 0, 11, 0, 255, 0, 0, 0, 255, 0, 31, 0, 0,
    0, 0, 12, 0, 0, 255, 0, 255, 0, 0, 30, 0, 0,
    0, 0, 13, 0, 0, 0, 255, 0, 0, 0, 29, 0, 0,
    0, 0, 14, 0, 0, 255, 0, 255, 0, 0, 28, 0, 0,
    0, 0, 15, 0, 255, 0, 0, 0, 255, 0, 27, 0, 0,
    0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 26, 0, 0,
    0, 0, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0, 0
};

std::uint8_t boundary_output_constant[] =
{
    1, 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 9,
    1, 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 9,
    1, 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 9,
    10, 10, 10, 0, 0, 0, 0, 0, 0, 0, 32, 32, 32,
    11, 11, 11, 0, 255, 0, 0, 0, 255, 0, 31, 31, 31,
    12, 12, 12, 0, 0, 255, 0, 255, 0, 0, 30, 30, 30,
    13, 13, 13, 0, 0, 0, 255, 0, 0, 0, 29, 29, 29,
    14, 14, 14, 0, 0, 255, 0, 255, 0, 0, 28, 28, 28,
    15, 15, 15, 0, 255, 0, 0, 0, 255, 0, 27, 27, 27,
    16, 16, 16, 0, 0, 0, 0, 0, 0, 0, 26, 26, 26,
    17, 17, 17, 18, 19, 20, 21, 22, 23, 24, 25, 25, 25,
    17, 17, 17, 18, 19, 20, 21, 22, 23, 24, 25, 25, 25,
    17, 17, 17, 18, 19, 20, 21, 22, 23, 24, 25, 25, 25
};

std::uint8_t boundary_output_zero[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0,
    0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0,
    0, 0, 11, 0, 255, 0, 0, 0, 255, 0, 31, 0, 0,
    0, 0, 12, 0, 0, 255, 0, 255, 0, 0, 30, 0, 0,
    0, 0, 13, 0, 0, 0, 255, 0, 0, 0, 29, 0, 0,
    0, 0, 14, 0, 0, 255, 0, 255, 0, 0, 28, 0, 0,
    0, 0, 15, 0, 255, 0, 0, 0, 255, 0, 27, 0, 0,
    0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 26, 0, 0,
    0, 0, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void test_extend_row_with_constant()
{
    gil::gray8c_view_t src_view =
        gil::interleaved_view(9, 9, reinterpret_cast<const gil::gray8_pixel_t *>(img), 9);

    gil::gray8c_view_t out_view =
        gil::interleaved_view(9, 13, reinterpret_cast<const gil::gray8_pixel_t *>(row_output_constant), 9);

    auto output = gil::extend_row(src_view, 2, gil::boundary_option::extend_constant);

    BOOST_TEST(gil::equal_pixels(out_view, gil::view(output)));
}

void test_extend_row_with_zero()
{
    gil::gray8c_view_t src_view =
        gil::interleaved_view(9, 9, reinterpret_cast<const gil::gray8_pixel_t *>(img), 9);

    gil::gray8c_view_t out_view =
        gil::interleaved_view(9, 15, reinterpret_cast<const gil::gray8_pixel_t *>(row_output_zero), 9);

    auto output = gil::extend_row(src_view, 3, gil::boundary_option::extend_zero);

    BOOST_TEST(gil::equal_pixels(out_view, gil::view(output)));
}

void test_extend_row_with_padded()
{
    gil::gray8c_view_t src_view =
        gil::interleaved_view(9, 9, reinterpret_cast<const gil::gray8_pixel_t *>(img), 9);

    auto src_sub_view = gil::subimage_view(src_view, 0, 1, 9, 7);

    auto output = gil::extend_row(src_sub_view, 1, gil::boundary_option::extend_padded);

    BOOST_TEST(gil::equal_pixels(src_view, gil::view(output)));
}

void test_extend_col_with_constant()
{
    gil::gray8c_view_t src_view =
        gil::interleaved_view(9, 9, reinterpret_cast<const gil::gray8_pixel_t *>(img), 9);

    gil::gray8c_view_t out_view =
        gil::interleaved_view(11, 9, reinterpret_cast<const gil::gray8_pixel_t *>(col_output_constant), 11);

    auto output = gil::extend_col(src_view, 1, gil::boundary_option::extend_constant);

    BOOST_TEST(gil::equal_pixels(out_view, gil::view(output)));
}

void test_extend_col_with_zero()
{
    gil::gray8c_view_t src_view =
        gil::interleaved_view(9, 9, reinterpret_cast<const gil::gray8_pixel_t *>(img), 9);

    gil::gray8c_view_t out_view =
        gil::interleaved_view(13, 9, reinterpret_cast<const gil::gray8_pixel_t *>(col_output_zero), 13);

    auto output = gil::extend_col(src_view, 2, gil::boundary_option::extend_zero);

    BOOST_TEST(gil::equal_pixels(out_view, gil::view(output)));
}

void test_extend_col_with_padded()
{
    gil::gray8c_view_t src_view =
        gil::interleaved_view(9, 9, reinterpret_cast<const gil::gray8_pixel_t *>(img), 9);

    auto src_sub_view = gil::subimage_view(src_view, 1, 0, 7, 9);

    auto output = gil::extend_col(src_sub_view, 1, gil::boundary_option::extend_padded);

    BOOST_TEST(gil::equal_pixels(src_view, gil::view(output)));
}

void test_extend_img_with_constant()
{
    gil::gray8c_view_t src_view =
        gil::interleaved_view(9, 9, reinterpret_cast<const gil::gray8_pixel_t *>(img), 9);

    gil::gray8c_view_t out_view =
        gil::interleaved_view(13, 13, reinterpret_cast<const gil::gray8_pixel_t *>(boundary_output_constant), 13);

    auto output = gil::extend_boundary(src_view, 2, gil::boundary_option::extend_constant);

    BOOST_TEST(gil::equal_pixels(out_view, gil::view(output)));
}

void test_extend_img_with_zero()
{
    gil::gray8c_view_t src_view =
        gil::interleaved_view(9, 9, reinterpret_cast<const gil::gray8_pixel_t *>(img), 9);

    gil::gray8c_view_t out_view =
        gil::interleaved_view(13, 13, reinterpret_cast<const gil::gray8_pixel_t *>(boundary_output_zero), 13);

    auto output = gil::extend_boundary(src_view, 2, gil::boundary_option::extend_zero);

    BOOST_TEST(gil::equal_pixels(out_view, gil::view(output)));
}

void test_extend_img_with_padded()
{
    gil::gray8c_view_t src_view =
        gil::interleaved_view(9, 9, reinterpret_cast<const gil::gray8_pixel_t *>(img), 9);

    auto src_sub_view = gil::subimage_view(src_view, 1, 1, 7, 7);

    auto output = gil::extend_boundary(src_sub_view, 1, gil::boundary_option::extend_padded);

    BOOST_TEST(gil::equal_pixels(src_view, gil::view(output)));
}

int main()
{
    test_extend_row_with_constant();
    test_extend_row_with_zero();
    test_extend_row_with_padded();
    test_extend_col_with_constant();
    test_extend_col_with_zero();
    test_extend_col_with_padded();
    test_extend_img_with_constant();
    test_extend_img_with_zero();
    test_extend_img_with_padded();

    return ::boost::report_errors();
}
