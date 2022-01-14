//
// Copyright 2019-2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
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

#include <boost/core/lightweight_test.hpp>

#include <limits>
#include <ostream>

#include "test_fixture.hpp"
#include "test_utility_output_stream.hpp"

namespace gil = boost::gil;
namespace fixture = boost::gil::test::fixture;

struct test_pixel_value_default_constructor
{
    template <typename Pixel>
    void operator()(Pixel const &)
    {
        using pixel_t = Pixel;
        fixture::pixel_value<pixel_t> fix;
        pixel_t const default_value{};
        BOOST_TEST_EQ(fix.pixel_, default_value);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::pixel_types>(test_pixel_value_default_constructor{});
    }
};

struct test_pixel_value_parameterized_constructor
{
    template <typename Pixel>
    void operator()(Pixel const &)
    {
        using pixel_t = Pixel;
        using channel_t = typename gil::channel_type<pixel_t>::type;
        // Sample channel value, simplified, could be min, max, random
        channel_t const sample_channel = 2;
        pixel_t sample_pixel;
        gil::static_fill(sample_pixel, sample_channel);
        fixture::pixel_value<pixel_t> fix{sample_pixel};
        BOOST_TEST_EQ(fix.pixel_, sample_pixel);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::pixel_types>(test_pixel_value_parameterized_constructor{});
    }
};

struct test_pixel_reference_default_constructor
{
        template <typename Pixel>
    void operator()(Pixel const &)
    {
        using pixel_t = Pixel;
        fixture::pixel_reference<pixel_t&> fix;
        pixel_t pix_default{};
        BOOST_TEST_EQ(fix.pixel_, pix_default);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::pixel_types>(test_pixel_reference_default_constructor{});
    }
};

struct test_pixel_reference_parameterized_constructor
{
    template <typename Pixel>
    void operator()(Pixel const &)
    {
        using pixel_t = Pixel;
        using channel_t = typename gil::channel_type<pixel_t>::type;
        // Sample channel value, simplified, could be min, max, random
        channel_t const sample_channel = 3;
        pixel_t sample_pixel;
        gil::static_fill(sample_pixel, sample_channel);
        fixture::pixel_reference<pixel_t&> fix{sample_pixel};
        BOOST_TEST_EQ(fix.pixel_, sample_pixel);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::pixel_types>(test_pixel_reference_parameterized_constructor{});
    }
};

int main()
{
    test_pixel_value_default_constructor::run();
    test_pixel_value_parameterized_constructor::run();
    test_pixel_reference_default_constructor::run();
    test_pixel_reference_parameterized_constructor::run();

    return boost::report_errors();
}
