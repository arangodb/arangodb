//
// Copyright 2018-2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/image.hpp>
#include <boost/gil/image_view.hpp>
#include <boost/gil/typedefs.hpp>

#include <boost/core/lightweight_test.hpp>

#include <iterator>

#include "test_utility_output_stream.hpp"

namespace gil = boost::gil;

namespace
{
gil::gray8_pixel_t const gray128(128);
gil::gray8_pixel_t const gray255(255);
} // unnamed namespace

// Collection

void test_begin()
{
    gil::gray8_image_t image(2, 2, gray255);
    auto view = gil::view(image);
    BOOST_TEST_EQ(*view.begin(), gray255);
}

void test_end()
{
    gil::gray8_image_t::view_t view;
    BOOST_TEST(view.begin() == view.end());
}

void test_empty()
{
    gil::gray8_image_t::view_t view;
    BOOST_TEST(view.empty());

    gil::gray8_image_t image(2, 2);
    view = gil::view(image);
    BOOST_TEST(!view.empty());
}

void test_size()
{
    gil::gray8_image_t::view_t view;
    BOOST_TEST_EQ(view.size(), 0);

    gil::gray8_image_t image(2, 2);
    view = gil::view(image);
    BOOST_TEST_EQ(view.size(), 4);
}

void test_swap()
{
    gil::gray8_image_t::view_t view1;
    gil::gray8_image_t::view_t view2;

    gil::gray8_image_t image(2, 2);
    view1 = gil::view(image);
    view1.swap(view2);

    BOOST_TEST(view1.empty());
    BOOST_TEST(!view2.empty());
}

// ForwardCollection

void test_back()
{
    gil::gray8_image_t image(2, 2, gray255);
    auto view = gil::view(image);
    BOOST_TEST_EQ(view.back(), gray255);
}

void test_front()
{
    gil::gray8_image_t image(2, 2, gray255);
    auto view = gil::view(image);
    BOOST_TEST_EQ(view.front(), gray255);
}

// ReversibleCollection

void test_rbegin()
{
    gil::gray8_image_t image(2, 2, gray255);
    auto view = gil::view(image);
    view(1,1) = gray128;
    BOOST_TEST_EQ(*view.rbegin(), gray128);
}

void test_rend()
{
    gil::gray8_image_t::view_t view;
    BOOST_TEST(view.rbegin() == view.rend());
}

int main()
{
    test_begin();
    test_end();
    test_empty();
    test_size();
    test_swap();
    test_back();
    test_front();
    test_rbegin();
    test_rend();

    return ::boost::report_errors();
}
