//
// Copyright 2019 Olzhas Zhumabek <anonymous.from.applecity@gmail.com>
//
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/gil/detail/math.hpp>
#include <boost/gil/image_processing/numeric.hpp>

#include <boost/core/lightweight_test.hpp>

#include <algorithm>

#include "test_utility_output_stream.hpp"

namespace gil = boost::gil;

void test_dx_sobel_kernel()
{
    auto const kernel = gil::generate_dx_sobel(1);
    BOOST_TEST_ALL_EQ(kernel.begin(), kernel.end(), gil::detail::dx_sobel.begin(), gil::detail::dx_sobel.end());
}

void test_dx_scharr_kernel()
{
    auto const kernel = gil::generate_dx_scharr(1);
    BOOST_TEST_ALL_EQ(kernel.begin(), kernel.end(), gil::detail::dx_scharr.begin(), gil::detail::dx_scharr.end());
}

void test_dy_sobel_kernel()
{
    auto const kernel = gil::generate_dy_sobel(1);
    BOOST_TEST_ALL_EQ(kernel.begin(), kernel.end(), gil::detail::dy_sobel.begin(), gil::detail::dy_sobel.end());
}

void test_dy_scharr_kernel()
{
    auto const kernel = gil::generate_dy_scharr(1);
    BOOST_TEST_ALL_EQ(kernel.begin(), kernel.end(), gil::detail::dy_scharr.begin(), gil::detail::dy_scharr.end());
}

int main()
{
    test_dx_sobel_kernel();
    test_dx_scharr_kernel();
    test_dy_sobel_kernel();
    test_dy_scharr_kernel();
    return boost::report_errors();
}
