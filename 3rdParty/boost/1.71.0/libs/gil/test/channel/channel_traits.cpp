//
// Copyright 2018 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/channel.hpp>
#include <boost/gil/typedefs.hpp>
#include <cstdint>
#include <limits>

#define BOOST_TEST_MODULE test_channel_traits
#include "unit_test.hpp"

namespace gil = boost::gil;

template <typename T>
void test_channel_minmax()
{
    BOOST_TEST(gil::channel_traits<T>::min_value() ==
               std::numeric_limits<T>::min());

    BOOST_TEST(gil::channel_traits<T>::max_value() ==
               std::numeric_limits<T>::max());
}

BOOST_AUTO_TEST_CASE(channel_minmax_uint8_t)
{
    test_channel_minmax<std::uint8_t>();
}

BOOST_AUTO_TEST_CASE(channel_minmax_int8_t)
{
    test_channel_minmax<std::int8_t>();
}

BOOST_AUTO_TEST_CASE(channel_minmax_uint16_t)
{
    test_channel_minmax<std::uint16_t>();
}

BOOST_AUTO_TEST_CASE(channel_minmax_int16_t)
{
    test_channel_minmax<std::int16_t>();
}

BOOST_AUTO_TEST_CASE(channel_minmax_uint32_t)
{
    test_channel_minmax<std::uint32_t>();
}

BOOST_AUTO_TEST_CASE(channel_minmax_int32_t)
{
    test_channel_minmax<std::int32_t>();
}
