//  Copyright (c) 2011 Helge Bahmann
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/atomic.hpp>

#include <boost/config.hpp>
#include <boost/cstdint.hpp>
#include <boost/core/lightweight_test.hpp>

#include "api_test_helpers.hpp"

int main(int, char *[])
{
    test_flag_api();

    test_integral_api<char>();
    test_integral_api<signed char>();
    test_integral_api<unsigned char>();
    test_integral_api<boost::uint8_t>();
    test_integral_api<boost::int8_t>();
    test_integral_api<short>();
    test_integral_api<unsigned short>();
    test_integral_api<boost::uint16_t>();
    test_integral_api<boost::int16_t>();
    test_integral_api<int>();
    test_integral_api<unsigned int>();
    test_integral_api<boost::uint32_t>();
    test_integral_api<boost::int32_t>();
    test_integral_api<long>();
    test_integral_api<unsigned long>();
    test_integral_api<boost::uint64_t>();
    test_integral_api<boost::int64_t>();
    test_integral_api<long long>();
    test_integral_api<unsigned long long>();
#if defined(BOOST_HAS_INT128)
    test_integral_api<boost::int128_type>();
    test_integral_api<boost::uint128_type>();
#endif

    test_constexpr_ctor<char>();
    test_constexpr_ctor<short>();
    test_constexpr_ctor<int>();
    test_constexpr_ctor<long>();
    test_constexpr_ctor<int*>();

    test_pointer_api<int>();

    test_enum_api();

    test_struct_api<test_struct<boost::uint8_t> >();
    test_struct_api<test_struct<boost::uint16_t> >();
    test_struct_api<test_struct<boost::uint32_t> >();
    test_struct_api<test_struct<boost::uint64_t> >();
#if defined(BOOST_HAS_INT128)
    test_struct_api<test_struct<boost::uint128_type> >();
#endif

    // https://svn.boost.org/trac/boost/ticket/10994
    test_struct_x2_api<test_struct_x2<boost::uint64_t> >();

    // https://svn.boost.org/trac/boost/ticket/9985
    test_struct_api<test_struct<double> >();

    test_large_struct_api();

    // Test that boost::atomic<T> only requires T to be trivially copyable.
    // Other non-trivial constructors are allowed.
    test_struct_with_ctor_api();

    return boost::report_errors();
}
