//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/config.hpp>
#include <boost/core/ignore_unused.hpp>

#if defined(BOOST_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wsign-conversion"
#elif BOOST_GCC >= 40700
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

#include <boost/gil/channel.hpp>

#include <limits>
#include <ostream>

#define BOOST_TEST_MODULE test_pixel_test_fixture
#include "unit_test.hpp"
#include "test_fixture.hpp"

namespace gil = boost::gil;
namespace fixture = boost::gil::test::fixture;

BOOST_AUTO_TEST_CASE_TEMPLATE(pixel_value_default_constructor, Pixel, fixture::pixel_types)
{
    fixture::pixel_value<Pixel> fix;
    Pixel const default_value{};
    // FIXME: Default value of pixel/homogeneous_color_base is undermined
    boost::ignore_unused(fix);
    boost::ignore_unused(default_value);
    //BOOST_TEST(fix.pixel_ == default_value);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(pixel_value_parameterized_constructor, Pixel, fixture::pixel_types)
{
    using channel_t = typename gil::channel_type<Pixel>::type;
    // Sample channel value, simplified, could be min, max, random
    channel_t const sample_channel = 2;
    Pixel sample_pixel;
    gil::static_fill(sample_pixel, sample_channel);
    fixture::pixel_value<Pixel> fix{sample_pixel};
    BOOST_TEST(fix.pixel_ == sample_pixel);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(pixel_reference_default_constructor, Pixel, fixture::pixel_types)
{
    fixture::pixel_reference<Pixel&> fix;
    Pixel const default_value{};
    // FIXME: Default value of pixel/homogeneous_color_base is undermined
    boost::ignore_unused(fix);
    boost::ignore_unused(default_value);
    //BOOST_TEST(fix.pixel_ == Pixel{});
}

BOOST_AUTO_TEST_CASE_TEMPLATE(pixel_reference_parameterized_constructor, Pixel, fixture::pixel_types)
{
    using channel_t = typename gil::channel_type<Pixel>::type;
    // Sample channel value, simplified, could be min, max, random
    channel_t const sample_channel = 3;
    Pixel sample_pixel;
    gil::static_fill(sample_pixel, sample_channel);
    fixture::pixel_reference<Pixel&> fix{sample_pixel};
    BOOST_TEST(fix.pixel_ == sample_pixel);
}
