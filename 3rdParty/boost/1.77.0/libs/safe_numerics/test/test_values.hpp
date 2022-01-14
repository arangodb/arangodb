#ifndef BOOST_SAFE_NUMERICS_TEST_VALUES_HPP
#define BOOST_SAFE_NUMERICS_TEST_VALUES_HPP

//  test_values.hpp
//
//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cstdint>
#include <type_traits> // integral_constant
#include <boost/mp11/list.hpp>
#include <boost/mp11/algorithm.hpp>

#define TEST_VALUE(type, value) std::integral_constant<type,(type)value>

using test_values = boost::mp11::mp_list<
    TEST_VALUE(std::int8_t,0x01),
    TEST_VALUE(std::int8_t,0x7f),
    TEST_VALUE(std::int8_t,0x80),
    TEST_VALUE(std::int8_t,0xff),
    TEST_VALUE(std::int16_t,0x0001),
    TEST_VALUE(std::int16_t,0x7fff),
    TEST_VALUE(std::int16_t,0x8000),
    TEST_VALUE(std::int16_t,0xffff),
    TEST_VALUE(std::int32_t,0x00000001),
    TEST_VALUE(std::int32_t,0x7fffffff),
    TEST_VALUE(std::int32_t,0x80000000),
    TEST_VALUE(std::int32_t,0xffffffff),
    TEST_VALUE(std::int64_t,0x0000000000000001),
    TEST_VALUE(std::int64_t,0x7fffffffffffffff),
    TEST_VALUE(std::int64_t,0x8000000000000000),
    TEST_VALUE(std::int64_t,0xffffffffffffffff),
    TEST_VALUE(std::uint8_t,0x01),
    TEST_VALUE(std::uint8_t,0x7f),
    TEST_VALUE(std::uint8_t,0x80),
    TEST_VALUE(std::uint8_t,0xff),
    TEST_VALUE(std::uint16_t,0x0001),
    TEST_VALUE(std::uint16_t,0x7fff),
    TEST_VALUE(std::uint16_t,0x8000),
    TEST_VALUE(std::uint16_t,0xffff),
    TEST_VALUE(std::uint32_t,0x00000001),
    TEST_VALUE(std::uint32_t,0x7fffffff),
    TEST_VALUE(std::uint32_t,0x80000000),
    TEST_VALUE(std::uint32_t,0xffffffff),
    TEST_VALUE(std::uint64_t,0x0000000000000001),
    TEST_VALUE(std::uint64_t,0x7fffffffffffffff),
    TEST_VALUE(std::uint64_t,0x8000000000000000),
    TEST_VALUE(std::uint64_t,0xffffffffffffffff),
    TEST_VALUE(std::int8_t,0x0) // uh oh - breaks test_types
>;

/*
template<typename T>
using extract_value_type = typename T::value_type;
using test_types = boost::mp11:: mp_unique<
    boost::mp11::mp_transform<
        extract_value_type,
        test_values
    >
>;
*/

using test_types = boost::mp11::mp_list<
    std::int8_t, std::int16_t, std::int32_t, std::int64_t,
    std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
>;

#endif
