//
// Copyright 2018-2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/algorithm.hpp>
#include <boost/gil/image.hpp>

#include <boost/core/lightweight_test.hpp>

#include "test_utility_output_stream.hpp"

namespace gil = boost::gil;

void test_lambda_expression()
{
    gil::gray8_pixel_t const gray128(128);
    gil::gray8_image_t image(2, 2, gray128);

    int sum{0};
    gil::for_each_pixel(gil::view(image), [&sum](gil::gray8_pixel_t& p) {
        sum += gil::at_c<0>(p);
    });
    BOOST_TEST_EQ(sum, 2 * 2 * 128);
}

int main()
{
    test_lambda_expression();

    return ::boost::report_errors();
}
