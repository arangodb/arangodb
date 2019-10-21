//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/channel.hpp>
#include <boost/gil/typedefs.hpp>
#include <cstdint>
#include <limits>
#include <type_traits>


#define BOOST_TEST_MODULE test_channel_traits
#include "unit_test.hpp"

namespace gil = boost::gil;

template <typename T>
void test_packed_channel_value_members()
{
    static_assert(std::is_same<typename T::value_type, T>::value,
        "value_type should be the same as packed_channel_value specialization");

    static_assert(std::is_lvalue_reference<typename T::reference>::value,
        "reference should be lvalue reference type");

    static_assert(std::is_lvalue_reference<typename T::reference>::value,
        "const_reference should be lvalue reference type");

    static_assert(std::is_pointer<typename T::pointer>::value,
        "pointer should be pointer type");

    static_assert(std::is_pointer<typename T::const_pointer>::value,
        "const_pointer should be pointer type");

    static_assert(T::is_mutable, "packed_channel_value should be mutable by default");

    static_assert(std::is_constructible<T, typename T::integer_t>::value,
        "packed_channel_value should be constructible from underlying integer_t");

    static_assert(std::is_convertible<T, typename T::integer_t>::value,
        "packed_channel_value should be convertible to underlying integer_t");
}

BOOST_AUTO_TEST_CASE(packed_channel_value_with_num_bits_1)
{
    using bits1 = gil::packed_channel_value<1>;

    test_packed_channel_value_members<bits1>();

    static_assert(std::is_same<bits1::integer_t, std::uint8_t>::value,
        "smallest integral type to store 1-bit value should be 8-bit unsigned");

    BOOST_TEST(bits1::num_bits() == 1u);
    BOOST_TEST(bits1::min_value() == 0u);
    BOOST_TEST(bits1::max_value() == 1u);
    BOOST_TEST(gil::channel_traits<bits1>::min_value() == 0u);
    BOOST_TEST(gil::channel_traits<bits1>::max_value() == 1u);
}

BOOST_AUTO_TEST_CASE(packed_channel_value_with_num_bits_8)
{
    using bits8 = gil::packed_channel_value<8>;

    test_packed_channel_value_members<bits8>();

    static_assert(std::is_same<bits8::integer_t, std::uint8_t>::value,
        "smallest integral type to store 8-bit value should be 8-bit unsigned");

    BOOST_TEST(bits8::num_bits() == 8u);
    BOOST_TEST(bits8::min_value() == 0u);
    BOOST_TEST(bits8::max_value() == 255u);
    BOOST_TEST(gil::channel_traits<bits8>::min_value() == 0u);
    BOOST_TEST(gil::channel_traits<bits8>::max_value() == 255u);
}

BOOST_AUTO_TEST_CASE(packed_channel_value_with_num_bits15)
{
    using bits15 = gil::packed_channel_value<15>;

    test_packed_channel_value_members<bits15>();

    static_assert(std::is_same<bits15::integer_t, std::uint16_t>::value,
        "smallest integral type to store 15-bit value should be 8-bit unsigned");

    BOOST_TEST(bits15::num_bits() == 15u);
    BOOST_TEST(bits15::min_value() == 0u);
    BOOST_TEST(bits15::max_value() == 32767u);
    BOOST_TEST(gil::channel_traits<bits15>::min_value() == 0u);
    BOOST_TEST(gil::channel_traits<bits15>::max_value() == 32767u);
}
