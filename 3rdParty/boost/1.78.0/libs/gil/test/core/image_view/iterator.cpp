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

void test_begin()
{
    {
        gil::gray8_image_t image = fixture::make_image_gray8();
        auto view = gil::view(image);
        BOOST_TEST_EQ(*view.begin(), fixture::gray8_draw_pixel);
    }
    {
        gil::rgb8_image_t image = fixture::make_image_rgb8();
        auto view = gil::view(image);
        BOOST_TEST_EQ(*view.begin(), fixture::rgb8_draw_pixel);
    }
}

void test_end()
{
    {
        gil::gray8_image_t image;
        auto view = gil::view(image);
        BOOST_TEST(view.begin() == view.end());
    }
    {
        gil::rgb8_image_t image;
        auto view = gil::view(image);
        BOOST_TEST(view.begin() == view.end());
    }
}

void test_at()
{
    {
        gil::gray8_image_t image = fixture::make_image_gray8();
        auto view = gil::view(image);
        // begin
        BOOST_TEST_EQ(*view.at(0), fixture::gray8_draw_pixel);
        BOOST_TEST_EQ(*view.at(1), fixture::gray8_back_pixel);
        BOOST_TEST_EQ(*view.at(0, 0), fixture::gray8_draw_pixel);
        BOOST_TEST_EQ(*view.at(0, 1), fixture::gray8_back_pixel);
        BOOST_TEST_EQ(*view.at(gil::point_t{0, 0}), fixture::gray8_draw_pixel);
        BOOST_TEST_EQ(*view.at(gil::point_t{0, 1}), fixture::gray8_back_pixel);
        // end
#ifdef NDEBUG // skip assertions
        BOOST_TEST(view.at(4) == view.end());
        // convoluted access to end iterator
        BOOST_TEST(view.at(0, 2) == view.end());
        BOOST_TEST(view.at(2, 1) == view.end());
#endif
    }
    {
        gil::rgb8_image_t image = fixture::make_image_rgb8();
        auto view = gil::view(image);
        // begin
        BOOST_TEST_EQ(*view.at(0), fixture::rgb8_draw_pixel);
        BOOST_TEST_EQ(*view.at(1), fixture::rgb8_back_pixel);
        BOOST_TEST_EQ(*view.at(0, 0), fixture::rgb8_draw_pixel);
        BOOST_TEST_EQ(*view.at(0, 1), fixture::rgb8_back_pixel);
        BOOST_TEST_EQ(*view.at(gil::point_t{0, 0}), fixture::rgb8_draw_pixel);
        BOOST_TEST_EQ(*view.at(gil::point_t{0, 1}), fixture::rgb8_back_pixel);
        // end
#ifdef NDEBUG // skip assertions
        BOOST_TEST(view.at(4) == view.end());
        // convoluted access to end iterator
        BOOST_TEST(view.at(0, 2) == view.end());
        BOOST_TEST(view.at(2, 1) == view.end());
#endif
    }
}

int main()
{
    test_begin();
    test_end();
    test_at();

    return boost::report_errors();
}
