//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/channel.hpp>
#include <boost/gil/typedefs.hpp>

namespace gil = boost::gil;

int main()
{
    static_assert(boost::is_integral<gil::packed_channel_value<1>>::value,
        "1-bit packed_channel_value should be recognized as integral");

    static_assert(boost::is_integral<gil::packed_channel_value<8>>::value,
        "8-bit packed_channel_value should be recognized as integral");

    static_assert(boost::is_integral
        <
            gil::packed_channel_reference
            <
                std::uint8_t, 0, 3, true
            >
        >::value,
        "3-bit packed_channel_reference should be recognized as integral");

    static_assert(boost::is_integral
        <
            gil::packed_dynamic_channel_reference
            <
                std::uint8_t, 2, true
            >
        >::value,
        "2-bit packed_dynamic_channel_reference should be recognized as integral");

    static_assert(boost::is_integral
        <
            gil::packed_dynamic_channel_reference
            <
                std::uint16_t, 15, true
            >
        >::value,
        "15-bit packed_dynamic_channel_reference should be recognized as integral");


    struct int_minus_value  { static std::int8_t apply() { return -64; } };
    struct int_plus_value   { static std::int8_t apply() { return  64; } };
    static_assert(boost::is_integral
        <
            gil::scoped_channel_value
            <
                std::uint8_t, int_minus_value, int_plus_value
            >
        >::value,
        "integer-based scoped_channel_value should be recognized as integral");

    static_assert(!boost::is_integral<gil::float32_t>::value,
        "float-based packed_channel_value should not be recognized as integral");

    static_assert(!boost::is_integral<gil::float64_t>::value,
        "float-based packed_channel_value should not be recognized as integral");
}
