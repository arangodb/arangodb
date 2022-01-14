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

void test_axis_0()
{
    {
        gil::gray8_image_t image = fixture::make_image_gray8();
        auto view = gil::view(image);
        // 0,0
        auto axit = view.axis_iterator<0>(gil::point_t{0, 0});
        BOOST_TEST_EQ(*axit, fixture::gray8_draw_pixel);
        BOOST_TEST(axit == view.row_begin(0));
        // 0,1
        ++axit;
        BOOST_TEST_EQ(*axit, fixture::gray8_back_pixel);
        // 1,1
        ++axit;
        BOOST_TEST_EQ(*axit, fixture::gray8_back_pixel);
        BOOST_TEST(axit == view.row_end(0));
        BOOST_TEST(axit == view.row_begin(1));
        // 1,1
        ++axit;
        BOOST_TEST_EQ(*axit, fixture::gray8_draw_pixel);
        // 2,0 - pass the end
        ++axit;
        BOOST_TEST(axit == view.row_end(1));
    }
    {
        gil::rgb8_image_t image = fixture::make_image_rgb8();
        auto view = gil::view(image);
        auto axit = view.axis_iterator<0>(gil::point_t{0, 0});
        BOOST_TEST(axit == view.row_begin(0));
        BOOST_TEST_EQ(*axit, fixture::rgb8_draw_pixel);
        // 0,1
        ++axit;
        BOOST_TEST_EQ(*axit, fixture::rgb8_back_pixel);
        // 1,1
        ++axit;
        BOOST_TEST(axit == view.row_end(0));
        BOOST_TEST_EQ(*axit, fixture::rgb8_back_pixel);
        BOOST_TEST(axit == view.row_begin(1));
        // 1,1
        ++axit;
        BOOST_TEST_EQ(*axit, fixture::rgb8_draw_pixel);
        // 2,0 - pass the end
        ++axit;
        BOOST_TEST(axit == view.row_end(1));
    }
}

void test_axis_1()
{
    {
        gil::gray8_image_t image = fixture::make_image_gray8();
        auto view = gil::view(image);
        // 0,0
        auto axit = view.axis_iterator<1>(gil::point_t{0, 0});
        BOOST_TEST(axit == view.col_begin(0));
        BOOST_TEST_EQ(*axit, fixture::gray8_draw_pixel);
        // 0,1
        ++axit;
        BOOST_TEST_EQ(*axit, fixture::gray8_back_pixel);
        // 0,2
        ++axit;
        BOOST_TEST(axit == view.col_end(0));
    }
}

int main()
{
    test_axis_0();
    test_axis_1();

    return boost::report_errors();
}
