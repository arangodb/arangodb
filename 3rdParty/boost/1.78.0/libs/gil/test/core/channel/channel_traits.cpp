//
// Copyright 2018-2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/channel.hpp>
#include <boost/gil/typedefs.hpp>

#include <boost/core/lightweight_test.hpp>

#include <cstdint>
#include <limits>

namespace gil = boost::gil;

template <typename T>
void test_channel_minmax()
{
    BOOST_TEST_EQ(gil::channel_traits<T>::min_value(), std::numeric_limits<T>::min());
    BOOST_TEST_EQ(gil::channel_traits<T>::max_value(), std::numeric_limits<T>::max());
}

void test_channel_minmax_uint8_t()
{
    test_channel_minmax<std::uint8_t>();
}

void test_channel_minmax_int8_t()
{
    test_channel_minmax<std::int8_t>();
}

void test_channel_minmax_uint16_t()
{
    test_channel_minmax<std::uint16_t>();
}

void test_channel_minmax_int16_t()
{
    test_channel_minmax<std::int16_t>();
}

void test_channel_minmax_uint32_t()
{
    test_channel_minmax<std::uint32_t>();
}

void test_channel_minmax_int32_t()
{
    test_channel_minmax<std::int32_t>();
}

int main()
{
    test_channel_minmax_uint8_t();
    test_channel_minmax_int8_t();
    test_channel_minmax_uint16_t();
    test_channel_minmax_int16_t();
    test_channel_minmax_uint32_t();
    test_channel_minmax_int32_t();

    return ::boost::report_errors();
}
