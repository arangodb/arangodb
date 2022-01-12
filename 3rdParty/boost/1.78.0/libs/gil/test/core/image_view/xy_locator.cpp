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

#include <boost/core/ignore_unused.hpp>
#include <boost/core/lightweight_test.hpp>

#include "test_fixture.hpp"
#include "test_utility_output_stream.hpp"

namespace gil = boost::gil;
namespace fixture = boost::gil::test::fixture;

void test_xy_at()
{
    {
        gil::gray8_image_t image = fixture::make_image_gray8();
        auto view = gil::view(image);
        BOOST_TEST_EQ(*view.xy_at(0, 0), fixture::gray8_draw_pixel);
        BOOST_TEST_EQ(*view.xy_at(1, 0), fixture::gray8_back_pixel);
        BOOST_TEST_EQ(*view.xy_at(0, 1), fixture::gray8_back_pixel);
        BOOST_TEST_EQ(*view.xy_at(1, 1), fixture::gray8_draw_pixel);
    }
    {
        gil::rgb8_image_t image = fixture::make_image_rgb8();
        auto view = gil::view(image);
        BOOST_TEST_EQ(*view.xy_at(0, 0), fixture::rgb8_draw_pixel);
        BOOST_TEST_EQ(*view.xy_at(1, 0), fixture::rgb8_back_pixel);
        BOOST_TEST_EQ(*view.xy_at(0, 1), fixture::rgb8_back_pixel);
        BOOST_TEST_EQ(*view.xy_at(1, 1), fixture::rgb8_draw_pixel);
    }
}

void test_xy_x_begin()
{
    {
        gil::gray8_image_t image = fixture::make_image_gray8();
        auto view = gil::view(image);
        BOOST_TEST(view.xy_at(0, 0).x() == view.row_begin(0));
        BOOST_TEST(view.xy_at(0, 1).x() == view.row_begin(1));
    }
    {
        gil::rgb8_image_t image = fixture::make_image_rgb8();
        auto view = gil::view(image);
        BOOST_TEST(view.xy_at(0, 0).x() == view.row_begin(0));
        BOOST_TEST(view.xy_at(0, 1).x() == view.row_begin(1));
    }
}

void test_xy_x_end()
{
    {
        gil::gray8_image_t image = fixture::make_image_gray8();
        auto view = gil::view(image);
#ifdef NDEBUG // skip assertion on x < width(), see TODO comment in image_view.hpp
        BOOST_TEST(view.xy_at(2, 0).x() == view.row_end(0));
        BOOST_TEST(view.xy_at(2, 1).x() == view.row_end(1));
#else
        boost::ignore_unused(view);
#endif
    }
    {
        gil::rgb8_image_t image = fixture::make_image_rgb8();
        auto view = gil::view(image);
#ifdef NDEBUG // skip assertion on x < width(), see TODO comment in image_view.hpp
        BOOST_TEST(view.xy_at(2, 0).x() == view.row_end(0));
        BOOST_TEST(view.xy_at(2, 1).x() == view.row_end(1));
#else
        boost::ignore_unused(view);
#endif
    }
}

void test_xy_y_begin()
{
    {
        gil::gray8_image_t image = fixture::make_image_gray8();
        auto view = gil::view(image);
        BOOST_TEST(view.xy_at(0, 0).y() == view.col_begin(0));
        BOOST_TEST(view.xy_at(1, 0).y() == view.col_begin(1));
    }
    {
        gil::rgb8_image_t image = fixture::make_image_rgb8();
        auto view = gil::view(image);
        BOOST_TEST(view.xy_at(0, 0).y() == view.col_begin(0));
        BOOST_TEST(view.xy_at(1, 0).y() == view.col_begin(1));
    }
}

void test_xy_y_end()
{
    {
        gil::gray8_image_t image = fixture::make_image_gray8();
        auto view = gil::view(image);
        BOOST_TEST(view.xy_at(0, 2).y() == view.col_end(0));
        BOOST_TEST(view.xy_at(1, 2).y() == view.col_end(1));
    }
    {
        gil::rgb8_image_t image = fixture::make_image_rgb8();
        auto view = gil::view(image);
        BOOST_TEST(view.xy_at(0, 2).y() == view.col_end(0));
        BOOST_TEST(view.xy_at(1, 2).y() == view.col_end(1));
    }
}

int main()
{
    test_xy_at();
    test_xy_x_begin();
    test_xy_x_end();
    test_xy_y_begin();
    test_xy_y_end();

    return boost::report_errors();
}
