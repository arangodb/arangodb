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

#include <vector>

#include "test_fixture.hpp"
#include "test_utility_output_stream.hpp"

namespace gil = boost::gil;
namespace fixture = boost::gil::test::fixture;

void test_y_at()
{
    {
        gil::gray8_image_t image = fixture::make_image_gray8();
        auto view = gil::view(image);
        BOOST_TEST_EQ(*view.y_at(0, 0), fixture::gray8_draw_pixel);
        BOOST_TEST_EQ(*view.y_at(1, 0), fixture::gray8_back_pixel);
        BOOST_TEST_EQ(*view.y_at(1, 1), fixture::gray8_draw_pixel);
        BOOST_TEST_EQ(*view.y_at(gil::point_t{0, 0}), fixture::gray8_draw_pixel);
        BOOST_TEST_EQ(*view.y_at(gil::point_t{0, 1}), fixture::gray8_back_pixel);
        BOOST_TEST_EQ(*view.y_at(gil::point_t{1, 1}), fixture::gray8_draw_pixel);
        BOOST_TEST(view.y_at(0, 0) == view.col_begin(0));
        BOOST_TEST(view.y_at(1, 0) == view.col_begin(1));
        BOOST_TEST(view.y_at(0, 2) == view.col_end(0));
        BOOST_TEST(view.y_at(1, 2) == view.col_end(1));
    }
    {
        gil::rgb8_image_t image = fixture::make_image_rgb8();
        auto view = gil::view(image);
        BOOST_TEST_EQ(*view.y_at(0, 0), fixture::rgb8_draw_pixel);
        BOOST_TEST_EQ(*view.y_at(1, 0), fixture::rgb8_back_pixel);
        BOOST_TEST_EQ(*view.y_at(1, 1), fixture::rgb8_draw_pixel);
        BOOST_TEST_EQ(*view.y_at(gil::point_t{0, 0}), fixture::rgb8_draw_pixel);
        BOOST_TEST_EQ(*view.y_at(gil::point_t{0, 1}), fixture::rgb8_back_pixel);
        BOOST_TEST_EQ(*view.y_at(gil::point_t{1, 1}), fixture::rgb8_draw_pixel);
        BOOST_TEST(view.y_at(0, 0) == view.col_begin(0));
        BOOST_TEST(view.y_at(1, 0) == view.col_begin(1));
        BOOST_TEST(view.y_at(0, 2) == view.col_end(0));
        BOOST_TEST(view.y_at(1, 2) == view.col_end(1));
    }
}

void test_col_begin()
{
    {
        gil::gray8_image_t image = fixture::make_image_gray8();
        auto view = gil::view(image);
        BOOST_TEST_EQ(*view.col_begin(0), fixture::gray8_draw_pixel);
        BOOST_TEST_EQ(*view.col_begin(1), fixture::gray8_back_pixel);
    }
    {
        gil::rgb8_image_t image = fixture::make_image_rgb8();
        auto view = gil::view(image);
        BOOST_TEST_EQ(*view.col_begin(0), fixture::rgb8_draw_pixel);
        BOOST_TEST_EQ(*view.col_begin(1), fixture::rgb8_back_pixel);
    }
}
void test_col_end()
{
    {
        gil::gray8_image_t image;
        auto view = gil::view(image);
#ifdef NDEBUG // skip assertion on x < width(), see TODO comment in image_view.hpp
        BOOST_TEST(view.col_begin(0) == view.col_end(0));
#else
        boost::ignore_unused(view);
#endif
    }
    {
        gil::rgb8_image_t image;
        auto view = gil::view(image);
#ifdef NDEBUG // skip assertion on x < width(), see TODO comment in image_view.hpp
        BOOST_TEST(view.col_begin(0) == view.col_end(0));
#else
        boost::ignore_unused(view);
#endif
    }
}

void test_issue_432()
{
    // Verifies https://github.com/boostorg/gil/pull/450 fix for https://github.com/boostorg/gil/issues/432
    // The tests were adapted from contribution of Daniel Evans by Olzhas Zhumabek.
    {
        std::vector<gil::gray8_pixel_t> v(50);
        auto view = boost::gil::interleaved_view(10, 5, v.data(), 10 * sizeof(gil::gray8_pixel_t));
        view.col_end(0); // BUG: Boost 1.72 always asserts
    }
    {
        std::vector<gil::rgb8_pixel_t> v(50);
        auto view = boost::gil::interleaved_view(10, 5, v.data(), 10 * sizeof(gil::rgb8_pixel_t));
        auto it = view.row_end(0); // BUG: Boost 1.72 always asserts
        boost::ignore_unused(it);
    }
}

int main()
{
    test_y_at();
    test_col_begin();
    test_col_end();

    test_issue_432();

    return boost::report_errors();
}
