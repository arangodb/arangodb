//
// Copyright 2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/image.hpp>
#include <boost/gil/image_view.hpp>
#include <boost/gil/typedefs.hpp>

#include <boost/core/lightweight_test.hpp>

#include "test_fixture.hpp"
#include "test_utility_output_stream.hpp"

namespace gil = boost::gil;
namespace fixture = boost::gil::test::fixture;

void test_rbegin()
{
    {
        gil::gray8_image_t image = fixture::make_image_gray8();
        auto view = gil::view(image);
        BOOST_TEST_EQ(*view.rbegin(), fixture::gray8_draw_pixel);
    }
    {
        gil::rgb8_image_t image = fixture::make_image_rgb8();
        auto view = gil::view(image);
        BOOST_TEST_EQ(*view.rbegin(), fixture::rgb8_draw_pixel);
    }
}

void test_rend()
{
    {
        gil::gray8_image_t image;
        auto view = gil::view(image);
        BOOST_TEST(view.rbegin() == view.rend());
    }
    {
        gil::rgb8_image_t image;
        auto view = gil::view(image);
        BOOST_TEST(view.rbegin() == view.rend());
    }
}

int main()
{
    test_rbegin();
    test_rend();

    return boost::report_errors();
}
