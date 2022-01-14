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

void test_x_at()
{
    {
        gil::gray8_image_t image = fixture::make_image_gray8();
        auto view = gil::view(image);
        BOOST_TEST_EQ(*view.x_at(0, 0), fixture::gray8_draw_pixel);
        BOOST_TEST_EQ(*view.x_at(1, 0), fixture::gray8_back_pixel);
        BOOST_TEST_EQ(*view.x_at(1, 1), fixture::gray8_draw_pixel);
        BOOST_TEST_EQ(*view.x_at(gil::point_t{0, 0}), fixture::gray8_draw_pixel);
        BOOST_TEST_EQ(*view.x_at(gil::point_t{0, 1}), fixture::gray8_back_pixel);
        BOOST_TEST_EQ(*view.x_at(gil::point_t{1, 1}), fixture::gray8_draw_pixel);
        BOOST_TEST_EQ(view.x_at(0, 0), view.row_begin(0));
        BOOST_TEST_EQ(view.x_at(0, 1), view.row_begin(1));
        BOOST_TEST_EQ(view.x_at(2, 0), view.row_end(0));
        BOOST_TEST_EQ(view.x_at(2, 1), view.row_end(1));
    }
    {
        gil::rgb8_image_t image = fixture::make_image_rgb8();
        auto view = gil::view(image);
        BOOST_TEST_EQ(*view.x_at(0, 0), fixture::rgb8_draw_pixel);
        BOOST_TEST_EQ(*view.x_at(1, 0), fixture::rgb8_back_pixel);
        BOOST_TEST_EQ(*view.x_at(1, 1), fixture::rgb8_draw_pixel);
        BOOST_TEST_EQ(*view.x_at(gil::point_t{0, 0}), fixture::rgb8_draw_pixel);
        BOOST_TEST_EQ(*view.x_at(gil::point_t{0, 1}), fixture::rgb8_back_pixel);
        BOOST_TEST_EQ(*view.x_at(gil::point_t{1, 1}), fixture::rgb8_draw_pixel);
        BOOST_TEST_EQ(view.x_at(0, 0), view.row_begin(0));
        BOOST_TEST_EQ(view.x_at(0, 1), view.row_begin(1));
        BOOST_TEST_EQ(view.x_at(2, 0), view.row_end(0));
        BOOST_TEST_EQ(view.x_at(2, 1), view.row_end(1));
    }
}

void test_row_begin()
{
    {
        gil::gray8_image_t image = fixture::make_image_gray8();
        auto view = gil::view(image);
        BOOST_TEST_EQ(*view.row_begin(0), fixture::gray8_draw_pixel);
        BOOST_TEST_EQ(*view.row_begin(1), fixture::gray8_back_pixel);
    }
    {
        gil::rgb8_image_t image = fixture::make_image_rgb8();
        auto view = gil::view(image);
        BOOST_TEST_EQ(*view.row_begin(0), fixture::rgb8_draw_pixel);
        BOOST_TEST_EQ(*view.row_begin(1), fixture::rgb8_back_pixel);
    }
}

void test_row_end()
{
    {
        gil::gray8_image_t image;
        auto view = gil::view(image);
#ifdef NDEBUG // skip assertion on y < height(), see TODO comment in image_view.hpp
        BOOST_TEST_EQ(view.row_begin(0), view.row_end(0));
#else
        boost::ignore_unused(view);
#endif
    }
    {
        gil::rgb8_image_t image;
        auto view = gil::view(image);
#ifdef NDEBUG // skip assertion on y < height(), see TODO comment in image_view.hpp
        BOOST_TEST_EQ(view.row_begin(0), view.row_end(0));
#else
        boost::ignore_unused(view);
#endif
    }
}

int main()
{
    test_x_at();
    test_row_begin();
    test_row_end();

    return boost::report_errors();
}
