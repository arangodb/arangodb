//
// Copyright 2019 Olzhas Zhumabek <anonymous.from.applecity@gmail.com>
//
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/gil/image.hpp>
#include <boost/gil/image_processing/numeric.hpp>
#include <boost/gil/image_view.hpp>
#include <boost/gil/typedefs.hpp>

#include <boost/core/lightweight_test.hpp>

namespace gil = boost::gil;

void test_normalized_mean_generation()
{
    auto kernel = gil::generate_normalized_mean(5);
    for (const auto& cell: kernel)
    {
        const auto expected_value = static_cast<float>(1 / 25.f);
        BOOST_TEST_EQ(cell, expected_value);
    }
}

void test_unnormalized_mean_generation()
{
    auto kernel = gil::generate_unnormalized_mean(5);
    for (const auto& cell: kernel)
    {
        BOOST_TEST_EQ(cell, 1.0f);
    }
}

void test_gaussian_kernel_generation()
{
    auto kernel = boost::gil::generate_gaussian_kernel(7, 0.84089642);
    const float expected_values[7][7] =
    {
        {0.00000067f, 0.00002292f, 0.00019117f, 0.00038771f, 0.00019117f, 0.00002292f, 0.00000067f},
        {0.00002292f, 0.00078633f, 0.00655965f, 0.01330373f, 0.00655965f, 0.00078633f, 0.00002292f},
        {0.00019117f, 0.00655965f, 0.05472157f, 0.11098164f, 0.05472157f, 0.00655965f, 0.00019117f},
        {0.00038771f, 0.01330373f, 0.11098164f, 0.25508352f, 0.11098164f, 0.01330373f, 0.00038711f},
        {0.00019117f, 0.00655965f, 0.05472157f, 0.11098164f, 0.05472157f, 0.00655965f, 0.00019117f},
        {0.00002292f, 0.00078633f, 0.00655965f, 0.01330373f, 0.00655965f, 0.00078633f, 0.00002292f},
        {0.00000067f, 0.00002292f, 0.00019117f, 0.00038771f, 0.00019117f, 0.00002292f, 0.00000067f}
    };

    for (gil::gray32f_view_t::coord_t y = 0; static_cast<std::size_t>(y) < kernel.size(); ++y)
    {
        for (gil::gray32f_view_t::coord_t x = 0; static_cast<std::size_t>(x) < kernel.size(); ++x)
        {
            auto output = kernel.at(static_cast<std::size_t>(x), static_cast<std::size_t>(y));
            auto expected = expected_values[y][x];
            auto percent_difference = std::ceil(std::abs(expected - output) / expected);
            BOOST_TEST_LT(percent_difference, 5);
        }
    }
}

int main()
{
    test_normalized_mean_generation();
    test_unnormalized_mean_generation();
    test_gaussian_kernel_generation();
    return boost::report_errors();
}
