//
// Copyright 2005-2007 Adobe Systems Incorporated
// Copyright 2018-2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distribtted under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/channel.hpp>
#include <boost/gil/channel_algorithm.hpp>
#include <boost/gil/typedefs.hpp>

#include <boost/core/lightweight_test.hpp>

#include <cmath>
#include <cstdint>
#include <limits>

namespace gil = boost::gil;

// FIXME: Remove when https://github.com/boostorg/core/issues/38 happens
#define BOOST_GIL_TEST_IS_CLOSE(a, b, epsilon) BOOST_TEST_LT(std::fabs((a) - (b)), (epsilon))

struct int_minus_value  { static std::int8_t apply() { return -64; } };
struct int_plus_value   { static std::int8_t apply() { return  64; } };
using fixture = gil::scoped_channel_value
    <
        std::uint8_t, int_minus_value, int_plus_value
    >;

void test_scoped_channel_value_default_constructor()
{
    fixture f;
    std::uint8_t v = f;
    BOOST_TEST_EQ(v, std::uint8_t{0});
}

void test_scoped_channel_value_user_defined_constructors()
{
    fixture f{1};
    std::uint8_t v = f;
    BOOST_TEST_EQ(v, std::uint8_t{1});
}

void test_scoped_channel_value_copy_constructors()
{
    fixture f1{128};
    fixture f2{f1};

    BOOST_TEST_EQ(std::uint8_t{f1}, std::uint8_t{128});
    BOOST_TEST_EQ(std::uint8_t{f1}, std::uint8_t{f2});
}

void test_scoped_channel_value_assignment()
{
    fixture f;
    f = 64;
    std::uint8_t v = f;
    BOOST_TEST_EQ(v, std::uint8_t{64});
}

void test_scoped_channel_value_float32_t()
{
    auto const epsilon = std::numeric_limits<float>::epsilon();
    // min
    BOOST_GIL_TEST_IS_CLOSE(gil::float_point_zero<float>::apply(), 0.0, epsilon);
    BOOST_TEST_EQ(gil::channel_traits<gil::float32_t>::min_value(), 0.0);
    // max
    BOOST_GIL_TEST_IS_CLOSE(gil::float_point_one<float>::apply(), 1.0, epsilon);
    BOOST_TEST_EQ(gil::channel_traits<gil::float32_t>::max_value(), 1.0);
}

void test_scoped_channel_value_float64_t()
{
    auto const epsilon = std::numeric_limits<double>::epsilon();
    // min
    BOOST_GIL_TEST_IS_CLOSE(gil::float_point_zero<double>::apply(), 0.0, epsilon);
    BOOST_GIL_TEST_IS_CLOSE(gil::channel_traits<gil::float64_t>::min_value(), 0.0, epsilon);
    // max
    BOOST_GIL_TEST_IS_CLOSE(gil::float_point_one<double>::apply(), 1.0, epsilon);
    BOOST_GIL_TEST_IS_CLOSE(gil::channel_traits<gil::float64_t>::max_value(), 1.0, epsilon);
}

void test_scoped_channel_value_halfs()
{
    // Create a double channel with range [-0.5 .. 0.5]
    struct minus_half { static double apply() { return -0.5; } };
    struct plus_half { static double apply() { return 0.5; } };
    using halfs = gil::scoped_channel_value<double, minus_half, plus_half>;

    auto const epsilon = std::numeric_limits<double>::epsilon();
    BOOST_GIL_TEST_IS_CLOSE(gil::channel_traits<halfs>::min_value(), minus_half::apply(), epsilon);
    BOOST_GIL_TEST_IS_CLOSE(gil::channel_traits<halfs>::max_value(), plus_half::apply(), epsilon);
    // scoped channel maximum should map to the maximum
    BOOST_GIL_TEST_IS_CLOSE(gil::channel_convert<std::uint16_t>(
        gil::channel_traits<halfs>::max_value()), 65535, epsilon);
}

int main()
{
    test_scoped_channel_value_default_constructor();
    test_scoped_channel_value_user_defined_constructors();
    test_scoped_channel_value_copy_constructors();
    test_scoped_channel_value_assignment();
    test_scoped_channel_value_float32_t();
    test_scoped_channel_value_float64_t();
    test_scoped_channel_value_halfs();

    return ::boost::report_errors();
}
