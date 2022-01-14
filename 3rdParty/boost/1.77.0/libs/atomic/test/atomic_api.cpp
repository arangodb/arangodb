//  Copyright (c) 2011 Helge Bahmann
//  Copyright (c) 2020 Andrey Semashev
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/atomic.hpp>

#include <boost/config.hpp>
#include <boost/cstdint.hpp>

#include "atomic_wrapper.hpp"
#include "api_test_helpers.hpp"

int main(int, char *[])
{
    test_flag_api< boost::atomic_flag >();

    test_integral_api< atomic_wrapper, char >();
    test_integral_api< atomic_wrapper, signed char >();
    test_integral_api< atomic_wrapper, unsigned char >();
    test_integral_api< atomic_wrapper, boost::uint8_t >();
    test_integral_api< atomic_wrapper, boost::int8_t >();
    test_integral_api< atomic_wrapper, short >();
    test_integral_api< atomic_wrapper, unsigned short >();
    test_integral_api< atomic_wrapper, boost::uint16_t >();
    test_integral_api< atomic_wrapper, boost::int16_t >();
    test_integral_api< atomic_wrapper, int >();
    test_integral_api< atomic_wrapper, unsigned int >();
    test_integral_api< atomic_wrapper, boost::uint32_t >();
    test_integral_api< atomic_wrapper, boost::int32_t >();
    test_integral_api< atomic_wrapper, long >();
    test_integral_api< atomic_wrapper, unsigned long >();
    test_integral_api< atomic_wrapper, boost::uint64_t >();
    test_integral_api< atomic_wrapper, boost::int64_t >();
    test_integral_api< atomic_wrapper, long long >();
    test_integral_api< atomic_wrapper, unsigned long long >();
#if defined(BOOST_HAS_INT128) && !defined(BOOST_ATOMIC_TESTS_NO_INT128)
    test_integral_api< atomic_wrapper, boost::int128_type >();
    test_integral_api< atomic_wrapper, boost::uint128_type >();
#endif

    test_constexpr_ctor< char >();
    test_constexpr_ctor< short >();
    test_constexpr_ctor< int >();
    test_constexpr_ctor< long >();
    // test_constexpr_ctor< int* >(); // for pointers we're not offering a constexpr constructor because of bitwise_cast

#if !defined(BOOST_ATOMIC_NO_FLOATING_POINT)
    test_floating_point_api< atomic_wrapper, float >();
    test_floating_point_api< atomic_wrapper, double >();
    test_floating_point_api< atomic_wrapper, long double >();
#if defined(BOOST_HAS_FLOAT128) && !defined(BOOST_ATOMIC_TESTS_NO_FLOAT128)
    test_floating_point_api< atomic_wrapper, boost::float128_type >();
#endif
#endif

    test_pointer_api< atomic_wrapper, int >();

    test_enum_api< atomic_wrapper >();

    test_struct_api< atomic_wrapper, test_struct< boost::uint8_t > >();
    test_struct_api< atomic_wrapper, test_struct< boost::uint16_t > >();
    test_struct_api< atomic_wrapper, test_struct< boost::uint32_t > >();
    test_struct_api< atomic_wrapper, test_struct< boost::uint64_t > >();
#if defined(BOOST_HAS_INT128)
    test_struct_api< atomic_wrapper, test_struct< boost::uint128_type > >();
#endif

    // https://svn.boost.org/trac/boost/ticket/10994
    test_struct_x2_api< atomic_wrapper, test_struct_x2< boost::uint64_t > >();

    // https://svn.boost.org/trac/boost/ticket/9985
    test_struct_api< atomic_wrapper, test_struct< double > >();

    test_large_struct_api< atomic_wrapper >();

    // Test that boost::atomic<T> only requires T to be trivially copyable.
    // Other non-trivial constructors are allowed.
    test_struct_with_ctor_api< atomic_wrapper >();

    // Test that fences at least compile
    boost::atomic_thread_fence(boost::memory_order_seq_cst);
    boost::atomic_signal_fence(boost::memory_order_seq_cst);

    return boost::report_errors();
}
