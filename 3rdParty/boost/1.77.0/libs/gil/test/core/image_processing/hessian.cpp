//
// Copyright 2019 Olzhas Zhumabek <anonymous.from.applecity@gmail.com>
//
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/gil/image.hpp>
#include <boost/gil/image_view.hpp>
#include <boost/gil/image_processing/numeric.hpp>
#include <boost/gil/image_processing/hessian.hpp>

#include <boost/core/lightweight_test.hpp>

namespace gil = boost::gil;

bool are_equal(gil::gray32f_view_t expected, gil::gray32f_view_t actual) {
    if (expected.dimensions() != actual.dimensions())
        return false;

    for (long int y = 0; y < expected.height(); ++y)
    {
        for (long int x = 0; x < expected.width(); ++x)
        {
            if (expected(x, y) != actual(x, y))
            {
                return false;
            }
        }
    }

    return true;
}

void test_blank_image()
{
    const gil::point_t dimensions(20, 20);
    gil::gray16s_image_t dx(dimensions, gil::gray16s_pixel_t(0), 0);
    gil::gray16s_image_t dy(dimensions, gil::gray16s_pixel_t(0), 0);

    gil::gray32f_image_t m11(dimensions);
    gil::gray32f_image_t m12_21(dimensions);
    gil::gray32f_image_t m22(dimensions);
    gil::gray32f_image_t expected(dimensions, gil::gray32f_pixel_t(0), 0);
    gil::compute_hessian_entries(
        gil::view(dx),
        gil::view(dy),
        gil::view(m11),
        gil::view(m12_21),
        gil::view(m22)
    );
    BOOST_TEST(are_equal(gil::view(expected), gil::view(m11)));
    BOOST_TEST(are_equal(gil::view(expected), gil::view(m12_21)));
    BOOST_TEST(are_equal(gil::view(expected), gil::view(m22)));

    gil::gray32f_image_t hessian_response(dimensions, gil::gray32f_pixel_t(0), 0);
    auto unnormalized_mean = gil::generate_unnormalized_mean(5);
    gil::compute_hessian_responses(
        gil::view(m11),
        gil::view(m12_21),
        gil::view(m22),
        unnormalized_mean,
        gil::view(hessian_response)
    );
    BOOST_TEST(are_equal(gil::view(expected), gil::view(hessian_response)));
}

int main(int argc, char* argv[])
{
    test_blank_image();
    return boost::report_errors();
}
