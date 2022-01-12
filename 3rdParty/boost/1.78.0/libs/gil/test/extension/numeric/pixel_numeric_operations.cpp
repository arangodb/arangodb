//
// Copyright 2019-2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/numeric/pixel_numeric_operations.hpp>

#include <boost/core/lightweight_test.hpp>

#include <tuple>
#include <type_traits>

#include "test_utility_output_stream.hpp"
#include "core/test_fixture.hpp" // random_value
#include "core/pixel/test_fixture.hpp"

namespace gil = boost::gil;
namespace fixture = boost::gil::test::fixture;

struct test_plus_integer_same_types
{
    template <typename Pixel>
    void operator()(Pixel const&)
    {
        using pixel_t = Pixel;
        using channel_t = typename gil::channel_type<pixel_t>::type;
        gil::pixel_plus_t<pixel_t, pixel_t, pixel_t> f;
        {
            pixel_t p0;
            gil::static_fill(p0, static_cast<channel_t>(0));
            BOOST_TEST_EQ(f(p0, p0), p0);
        }
        {
            pixel_t p1;
            gil::static_fill(p1, static_cast<channel_t>(1));
            pixel_t r2;
            gil::static_fill(r2, static_cast<channel_t>(2));
            BOOST_TEST_EQ(f(p1, p1), r2);
        }
        {
            // Generates pixels with consecutive channel values: {1} or {1,2,3} or {1,2,3,4} etc.
            fixture::consecutive_value<channel_t> g(1);
            pixel_t p;
            gil::static_generate(p, [&g]() { return g(); });
            auto const r = f(p, p);
            BOOST_TEST_NE(r, p);
            BOOST_TEST_EQ(gil::at_c<0>(r), (gil::at_c<0>(p) + gil::at_c<0>(p)));
        }
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::pixel_integer_types>(test_plus_integer_same_types{});
    }
};

struct test_minus_integer_same_types
{
    template <typename Pixel>
    void operator()(Pixel const&)
    {
        using pixel_t = Pixel;
        using channel_t = typename gil::channel_type<pixel_t>::type;
        gil::pixel_minus_t<pixel_t, pixel_t, pixel_t> f;

        pixel_t p0;
        gil::static_fill(p0, static_cast<channel_t>(0));
        BOOST_TEST_EQ(f(p0, p0), p0);
        {
            pixel_t p1, p2;
            gil::static_fill(p1, static_cast<channel_t>(1));
            gil::static_fill(p2, static_cast<channel_t>(2));
            pixel_t r1;
            gil::static_fill(r1, static_cast<channel_t>(1));
            BOOST_TEST_EQ(f(p2, p1), r1);
        }
        {
            // Generates pixels with consecutive channel values: {1} or {1,2,3} or {1,2,3,4} etc.
            fixture::consecutive_value<channel_t> g(1);
            pixel_t p;
            gil::static_generate(p, [&g]() { return g(); });
            BOOST_TEST_EQ(f(p, p), p0);
        }
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::pixel_integer_types>(test_minus_integer_same_types{});
    }
};

struct test_pixel_multiplies_scalar_integer_same_types
{
    template <typename Pixel>
    void operator()(Pixel const&)
    {
        using pixel_t = Pixel;
        using channel_t = typename gil::channel_type<pixel_t>::type;
        gil::pixel_multiplies_scalar_t<pixel_t, channel_t, pixel_t> f;

        pixel_t p0;
        gil::static_fill(p0, static_cast<channel_t>(0));
        BOOST_TEST_EQ(f(p0, 0), p0);

        {
            pixel_t p1;
            gil::static_fill(p1, static_cast<channel_t>(1));
            BOOST_TEST_EQ(f(p1, 0), p0);
            BOOST_TEST_EQ(f(p1, 1), p1);
        }
        {
            // Generates pixels with consecutive channel values: {1} or {1,2,3} or {1,2,3,4} etc.
            fixture::consecutive_value<channel_t> g(1);
            pixel_t p;
            gil::static_generate(p, [&g]() { return g(); });

            // check first channel value is doubled
            auto const r = f(p, 2);
            BOOST_TEST_NE(r, p);
            BOOST_TEST_EQ(gil::at_c<0>(r), (gil::at_c<0>(p) * 2));
        }
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::pixel_integer_types>(test_pixel_multiplies_scalar_integer_same_types{});
    }
};

struct test_pixel_multiply_integer_same_types
{
    template <typename Pixel>
    void operator()(Pixel const&)
    {
        using pixel_t = Pixel;
        using channel_t = typename gil::channel_type<pixel_t>::type;
        gil::pixel_multiply_t<pixel_t, pixel_t, pixel_t> f;

        pixel_t p0;
        gil::static_fill(p0, static_cast<channel_t>(0));
        BOOST_TEST_EQ(f(p0, p0), p0);

        pixel_t p1;
        gil::static_fill(p1, static_cast<channel_t>(1));
        BOOST_TEST_EQ(f(p1, p1), p1);

        pixel_t p2;
        gil::static_fill(p2, static_cast<channel_t>(2));
        BOOST_TEST_EQ(f(p1, p2), p2);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::pixel_integer_types>(test_pixel_multiply_integer_same_types{});
    }
};

struct test_pixel_divides_scalar_integer_same_types
{
    template <typename Pixel>
    void operator()(Pixel const&)
    {
        using pixel_t = Pixel;
        using channel_t = typename gil::channel_type<pixel_t>::type;
        gil::pixel_divides_scalar_t<pixel_t, channel_t, pixel_t> f;

        pixel_t p0;
        gil::static_fill(p0, static_cast<channel_t>(0));
        BOOST_TEST_EQ(f(p0, 1), p0);

        pixel_t p1;
        gil::static_fill(p1, static_cast<channel_t>(1));
        BOOST_TEST_EQ(f(p1, 1), p1);

        pixel_t p2;
        gil::static_fill(p2, static_cast<channel_t>(2));
        BOOST_TEST_EQ(f(p2, 2), p1);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::pixel_integer_types>(test_pixel_divides_scalar_integer_same_types{});
    }
};

struct test_pixel_divide_integer_same_types
{
    template <typename Pixel>
    void operator()(Pixel const&)
    {
        using pixel_t = Pixel;
        using channel_t = typename gil::channel_type<pixel_t>::type;
        gil::pixel_divide_t<pixel_t, pixel_t, pixel_t> f;

        pixel_t p0;
        gil::static_fill(p0, static_cast<channel_t>(0));
        pixel_t p1;
        gil::static_fill(p1, static_cast<channel_t>(1));
        BOOST_TEST_EQ(f(p0, p1), p0);
        BOOST_TEST_EQ(f(p1, p1), p1);

        pixel_t p2;
        gil::static_fill(p2, static_cast<channel_t>(2));
        BOOST_TEST_EQ(f(p2, p1), p2);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::pixel_integer_types>(test_pixel_divide_integer_same_types{});
    }
};

struct test_pixel_halves_integer_same_types
{
    template <typename Pixel>
    void operator()(Pixel const&)
    {
        using pixel_t = Pixel;
        using channel_t = typename gil::channel_type<pixel_t>::type;
        gil::pixel_halves_t<pixel_t> f;

        pixel_t p0;
        gil::static_fill(p0, static_cast<channel_t>(0));
        pixel_t p1;
        gil::static_fill(p1, static_cast<channel_t>(1));

        {
            auto p = p0;
            BOOST_TEST_EQ(f(p), p0);
        }
        {
            auto p = p1;
            BOOST_TEST_EQ(f(p), p0); // truncates toward Zero
        }
        {
            pixel_t p2;
            gil::static_fill(p2, static_cast<channel_t>(2));
            BOOST_TEST_EQ(f(p2), p1);
        }
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::pixel_integer_types>(test_pixel_halves_integer_same_types{});
    }
};

struct test_pixel_zeros_integer_same_types
{
    template <typename Pixel>
    void operator()(Pixel const&)
    {
        using pixel_t = Pixel;
        using channel_t = typename gil::channel_type<pixel_t>::type;
        gil::pixel_zeros_t<pixel_t> f;

        pixel_t p0;
        gil::static_fill(p0, static_cast<channel_t>(0));
        {
            auto p = p0;
            BOOST_TEST_EQ(f(p), p0);
        }
        {
            fixture::consecutive_value<channel_t> g(1);
            pixel_t p;
            gil::static_generate(p, [&g]() { return g(); });
            BOOST_TEST_EQ(f(p), p0);
        }
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::pixel_integer_types>(test_pixel_zeros_integer_same_types{});
    }
};

struct test_zero_channels_integer_same_types
{
    template <typename Pixel>
    void operator()(Pixel const&)
    {
        using pixel_t = Pixel;
        using channel_t = typename gil::channel_type<pixel_t>::type;

        pixel_t p0;
        gil::static_fill(p0, static_cast<channel_t>(0));
        {
            auto p = p0;
            gil::zero_channels(p);
            BOOST_TEST_EQ(p, p0);
        }
        {
            fixture::consecutive_value<channel_t> g(1);
            pixel_t p;
            gil::static_generate(p, [&g]() { return g(); });
            gil::zero_channels(p);
            BOOST_TEST_EQ(p, p0);
        }
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::pixel_integer_types>(test_zero_channels_integer_same_types{});
    }
};

struct test_pixel_assigns_integer_same_types
{
    template <typename Pixel>
    void operator()(Pixel const&)
    {
        using pixel_t = Pixel;
        using channel_t = typename gil::channel_type<pixel_t>::type;
        gil::pixel_assigns_t<pixel_t, pixel_t> f;

        {
            pixel_t p0, r;
            gil::static_fill(p0, static_cast<channel_t>(0));
            f(p0, r);
            BOOST_TEST_EQ(p0, r);
        }
        {
            fixture::consecutive_value<channel_t> g(1);
            pixel_t p, r;
            gil::static_generate(p, [&g]() { return g(); });
            f(p, r);
            BOOST_TEST_EQ(p, r);
        }
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::pixel_integer_types>(test_pixel_assigns_integer_same_types{});
    }
};
int main()
{
    test_plus_integer_same_types::run();
    test_minus_integer_same_types::run();
    test_pixel_multiplies_scalar_integer_same_types::run();
    test_pixel_multiply_integer_same_types::run();
    test_pixel_divides_scalar_integer_same_types::run();
    test_pixel_divide_integer_same_types::run();
    test_pixel_halves_integer_same_types::run();
    test_pixel_zeros_integer_same_types::run();
    test_zero_channels_integer_same_types::run();
    test_pixel_assigns_integer_same_types::run();

    return ::boost::report_errors();
}
