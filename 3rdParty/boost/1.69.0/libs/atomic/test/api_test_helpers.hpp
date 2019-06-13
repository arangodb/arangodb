//  Copyright (c) 2011 Helge Bahmann
//  Copyright (c) 2017 - 2018 Andrey Semashev
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_ATOMIC_API_TEST_HELPERS_HPP
#define BOOST_ATOMIC_API_TEST_HELPERS_HPP

#include <boost/atomic.hpp>
#include <cstddef>
#include <cstring>
#include <limits>
#include <iostream>
#include <boost/config.hpp>
#include <boost/cstdint.hpp>
#include <boost/type_traits/integral_constant.hpp>
#include <boost/type_traits/is_signed.hpp>
#include <boost/type_traits/is_unsigned.hpp>

struct test_stream_type
{
    typedef std::ios_base& (*ios_base_manip)(std::ios_base&);
    typedef std::basic_ios< char, std::char_traits< char > >& (*basic_ios_manip)(std::basic_ios< char, std::char_traits< char > >&);
    typedef std::ostream& (*stream_manip)(std::ostream&);

    template< typename T >
    test_stream_type const& operator<< (T const& value) const
    {
        std::cerr << value;
        return *this;
    }

    test_stream_type const& operator<< (ios_base_manip manip) const
    {
        std::cerr << manip;
        return *this;
    }
    test_stream_type const& operator<< (basic_ios_manip manip) const
    {
        std::cerr << manip;
        return *this;
    }
    test_stream_type const& operator<< (stream_manip manip) const
    {
        std::cerr << manip;
        return *this;
    }

    // Make sure characters are printed as numbers if tests fail
    test_stream_type const& operator<< (char value) const
    {
        std::cerr << static_cast< int >(value);
        return *this;
    }
    test_stream_type const& operator<< (signed char value) const
    {
        std::cerr << static_cast< int >(value);
        return *this;
    }
    test_stream_type const& operator<< (unsigned char value) const
    {
        std::cerr << static_cast< unsigned int >(value);
        return *this;
    }
    test_stream_type const& operator<< (short value) const
    {
        std::cerr << static_cast< int >(value);
        return *this;
    }
    test_stream_type const& operator<< (unsigned short value) const
    {
        std::cerr << static_cast< unsigned int >(value);
        return *this;
    }

#if defined(BOOST_HAS_INT128)
    // Some GCC versions don't provide output operators for __int128
    test_stream_type const& operator<< (boost::int128_type const& v) const
    {
        std::cerr << static_cast< long long >(v);
        return *this;
    }
    test_stream_type const& operator<< (boost::uint128_type const& v) const
    {
        std::cerr << static_cast< unsigned long long >(v);
        return *this;
    }
#endif // defined(BOOST_HAS_INT128)
#if defined(BOOST_HAS_FLOAT128)
    // libstdc++ does not provide output operators for __float128
    test_stream_type const& operator<< (boost::float128_type const& v) const
    {
        std::cerr << static_cast< double >(v);
        return *this;
    }
#endif // defined(BOOST_HAS_FLOAT128)
};

const test_stream_type test_stream = {};

#define BOOST_LIGHTWEIGHT_TEST_OSTREAM test_stream

#include <boost/core/lightweight_test.hpp>

#include "value_with_epsilon.hpp"

/* provide helpers that exercise whether the API
functions of "boost::atomic" provide the correct
operational semantic in the case of sequential
execution */

static void
test_flag_api(void)
{
#ifndef BOOST_ATOMIC_NO_ATOMIC_FLAG_INIT
    boost::atomic_flag f = BOOST_ATOMIC_FLAG_INIT;
#else
    boost::atomic_flag f;
#endif

    BOOST_TEST( !f.test_and_set() );
    BOOST_TEST( f.test_and_set() );
    f.clear();
    BOOST_TEST( !f.test_and_set() );
}

template<typename T>
void test_base_operators(T value1, T value2, T value3)
{
    /* explicit load/store */
    {
        boost::atomic<T> a(value1);
        BOOST_TEST_EQ( a.load(), value1 );
    }

    {
        boost::atomic<T> a(value1);
        a.store(value2);
        BOOST_TEST_EQ( a.load(), value2 );
    }

    /* overloaded assignment/conversion */
    {
        boost::atomic<T> a(value1);
        BOOST_TEST( value1 == a );
    }

    {
        boost::atomic<T> a;
        a = value2;
        BOOST_TEST( value2 == a );
    }

    /* exchange-type operators */
    {
        boost::atomic<T> a(value1);
        T n = a.exchange(value2);
        BOOST_TEST_EQ( a.load(), value2 );
        BOOST_TEST_EQ( n, value1 );
    }

    {
        boost::atomic<T> a(value1);
        T expected = value1;
        bool success = a.compare_exchange_strong(expected, value3);
        BOOST_TEST( success );
        BOOST_TEST_EQ( a.load(), value3 );
        BOOST_TEST_EQ( expected, value1 );
    }

    {
        boost::atomic<T> a(value1);
        T expected = value2;
        bool success = a.compare_exchange_strong(expected, value3);
        BOOST_TEST( !success );
        BOOST_TEST_EQ( a.load(), value1 );
        BOOST_TEST_EQ( expected, value1 );
    }

    {
        boost::atomic<T> a(value1);
        T expected;
        bool success;
        do {
            expected = value1;
            success = a.compare_exchange_weak(expected, value3);
        } while(!success);
        BOOST_TEST( success );
        BOOST_TEST_EQ( a.load(), value3 );
        BOOST_TEST_EQ( expected, value1 );
    }

    {
        boost::atomic<T> a(value1);
        T expected;
        bool success;
        do {
            expected = value2;
            success = a.compare_exchange_weak(expected, value3);
            if (expected != value2)
                break;
        } while(!success);
        BOOST_TEST( !success );
        BOOST_TEST_EQ( a.load(), value1 );
        BOOST_TEST_EQ( expected, value1 );
    }
}

// T requires an int constructor
template <typename T>
void test_constexpr_ctor()
{
#ifndef BOOST_NO_CXX11_CONSTEXPR
    const T value(0);
    const boost::atomic<T> tester(value);
    BOOST_TEST( tester == value );
#endif
}

//! The type traits provides max and min values of type D that can be added/subtracted to T(0) without signed overflow
template< typename T, typename D, bool IsSigned = boost::is_signed< D >::value >
struct distance_limits
{
    static D min BOOST_PREVENT_MACRO_SUBSTITUTION () BOOST_NOEXCEPT
    {
        return (std::numeric_limits< D >::min)();
    }
    static D max BOOST_PREVENT_MACRO_SUBSTITUTION () BOOST_NOEXCEPT
    {
        return (std::numeric_limits< D >::max)();
    }
};

#if defined(BOOST_MSVC)
#pragma warning(push)
// 'static_cast': truncation of constant value. There is no actual truncation happening because
// the cast is only performed if the value fits in the range of the result.
#pragma warning(disable: 4309)
#endif

template< typename T, typename D >
struct distance_limits< T*, D, true >
{
    static D min BOOST_PREVENT_MACRO_SUBSTITUTION () BOOST_NOEXCEPT
    {
        const std::ptrdiff_t ptrdiff = (std::numeric_limits< std::ptrdiff_t >::min)() / static_cast< std::ptrdiff_t >(sizeof(T));
        const D diff = (std::numeric_limits< D >::min)();
        // Both values are negative. Return the closest value to zero.
        return diff < ptrdiff ? static_cast< D >(ptrdiff) : diff;
    }
    static D max BOOST_PREVENT_MACRO_SUBSTITUTION () BOOST_NOEXCEPT
    {
        const std::ptrdiff_t ptrdiff = (std::numeric_limits< std::ptrdiff_t >::max)() / static_cast< std::ptrdiff_t >(sizeof(T));
        const D diff = (std::numeric_limits< D >::max)();
        // Both values are positive. Return the closest value to zero.
        return diff > ptrdiff ? static_cast< D >(ptrdiff) : diff;
    }
};

template< typename T, typename D >
struct distance_limits< T*, D, false >
{
    static D min BOOST_PREVENT_MACRO_SUBSTITUTION () BOOST_NOEXCEPT
    {
        return (std::numeric_limits< D >::min)();
    }
    static D max BOOST_PREVENT_MACRO_SUBSTITUTION () BOOST_NOEXCEPT
    {
        const std::size_t ptrdiff = static_cast< std::size_t >((std::numeric_limits< std::ptrdiff_t >::max)()) / sizeof(T);
        const D diff = (std::numeric_limits< D >::max)();
        return diff > ptrdiff ? static_cast< D >(ptrdiff) : diff;
    }
};

#if defined(BOOST_HAS_INT128)

// At least libstdc++ does not specialize std::numeric_limits for __int128 in strict mode (i.e. with GNU extensions disabled).
// So we have to specialize the limits ourself. We assume two's complement signed representation.
template< typename T, bool IsSigned >
struct distance_limits< T, boost::int128_type, IsSigned >
{
    static boost::int128_type min BOOST_PREVENT_MACRO_SUBSTITUTION () BOOST_NOEXCEPT
    {
        return -(max)() - 1;
    }
    static boost::int128_type max BOOST_PREVENT_MACRO_SUBSTITUTION () BOOST_NOEXCEPT
    {
        return static_cast< boost::int128_type >((~static_cast< boost::uint128_type >(0u)) >> 1);
    }
};

template< typename T, bool IsSigned >
struct distance_limits< T, boost::uint128_type, IsSigned >
{
    static boost::uint128_type min BOOST_PREVENT_MACRO_SUBSTITUTION () BOOST_NOEXCEPT
    {
        return 0u;
    }
    static boost::uint128_type max BOOST_PREVENT_MACRO_SUBSTITUTION () BOOST_NOEXCEPT
    {
        return ~static_cast< boost::uint128_type >(0u);
    }
};

#endif // defined(BOOST_HAS_INT128)

#if defined(BOOST_MSVC)
#pragma warning(pop)
#endif

template<typename T, typename D, typename AddType>
void test_additive_operators_with_type_and_test()
{
    // Note: This set of tests is extracted to a separate function because otherwise MSVC-10 for x64 generates broken code
    const T zero_value = 0;
    const D zero_diff = 0;
    const D one_diff = 1;
    const AddType zero_add = 0;
    {
        boost::atomic<T> a(zero_value);
        bool f = a.add_and_test(zero_diff);
        BOOST_TEST_EQ( f, false );
        BOOST_TEST_EQ( a.load(), zero_value );

        f = a.add_and_test(one_diff);
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), T(zero_add + one_diff) );
    }
    {
        boost::atomic<T> a(zero_value);
        bool f = a.add_and_test((distance_limits< T, D >::max)());
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), T(zero_add + (distance_limits< T, D >::max)()) );
    }
    {
        boost::atomic<T> a(zero_value);
        bool f = a.add_and_test((distance_limits< T, D >::min)());
        BOOST_TEST_EQ( f, ((distance_limits< T, D >::min)() != 0) );
        BOOST_TEST_EQ( a.load(), T(zero_add + (distance_limits< T, D >::min)()) );
    }

    {
        boost::atomic<T> a(zero_value);
        bool f = a.sub_and_test(zero_diff);
        BOOST_TEST_EQ( f, false );
        BOOST_TEST_EQ( a.load(), zero_value );

        f = a.sub_and_test(one_diff);
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), T(zero_add - one_diff) );
    }
    {
        boost::atomic<T> a(zero_value);
        bool f = a.sub_and_test((distance_limits< T, D >::max)());
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), T(zero_add - (distance_limits< T, D >::max)()) );
    }
    {
        boost::atomic<T> a(zero_value);
        bool f = a.sub_and_test((distance_limits< T, D >::min)());
        BOOST_TEST_EQ( f, ((distance_limits< T, D >::min)() != 0) );
        BOOST_TEST_EQ( a.load(), T(zero_add - (distance_limits< T, D >::min)()) );
    }
}

template<typename T, typename D, typename AddType>
void test_additive_operators_with_type(T value, D delta)
{
    /* note: the tests explicitly cast the result of any addition
    to the type to be tested to force truncation of the result to
    the correct range in case of overflow */

    /* explicit add/sub */
    {
        boost::atomic<T> a(value);
        T n = a.fetch_add(delta);
        BOOST_TEST_EQ( a.load(), T((AddType)value + delta) );
        BOOST_TEST_EQ( n, value );
    }

    {
        boost::atomic<T> a(value);
        T n = a.fetch_sub(delta);
        BOOST_TEST_EQ( a.load(), T((AddType)value - delta) );
        BOOST_TEST_EQ( n, value );
    }

    /* overloaded modify/assign*/
    {
        boost::atomic<T> a(value);
        T n = (a += delta);
        BOOST_TEST_EQ( a.load(), T((AddType)value + delta) );
        BOOST_TEST_EQ( n, T((AddType)value + delta) );
    }

    {
        boost::atomic<T> a(value);
        T n = (a -= delta);
        BOOST_TEST_EQ( a.load(), T((AddType)value - delta) );
        BOOST_TEST_EQ( n, T((AddType)value - delta) );
    }

    /* overloaded increment/decrement */
    {
        boost::atomic<T> a(value);
        T n = a++;
        BOOST_TEST_EQ( a.load(), T((AddType)value + 1) );
        BOOST_TEST_EQ( n, value );
    }

    {
        boost::atomic<T> a(value);
        T n = ++a;
        BOOST_TEST_EQ( a.load(), T((AddType)value + 1) );
        BOOST_TEST_EQ( n, T((AddType)value + 1) );
    }

    {
        boost::atomic<T> a(value);
        T n = a--;
        BOOST_TEST_EQ( a.load(), T((AddType)value - 1) );
        BOOST_TEST_EQ( n, value );
    }

    {
        boost::atomic<T> a(value);
        T n = --a;
        BOOST_TEST_EQ( a.load(), T((AddType)value - 1) );
        BOOST_TEST_EQ( n, T((AddType)value - 1) );
    }

    // Operations returning the actual resulting value
    {
        boost::atomic<T> a(value);
        T n = a.add(delta);
        BOOST_TEST_EQ( a.load(), T((AddType)value + delta) );
        BOOST_TEST_EQ( n, T((AddType)value + delta) );
    }

    {
        boost::atomic<T> a(value);
        T n = a.sub(delta);
        BOOST_TEST_EQ( a.load(), T((AddType)value - delta) );
        BOOST_TEST_EQ( n, T((AddType)value - delta) );
    }

    // Opaque operations
    {
        boost::atomic<T> a(value);
        a.opaque_add(delta);
        BOOST_TEST_EQ( a.load(), T((AddType)value + delta) );
    }

    {
        boost::atomic<T> a(value);
        a.opaque_sub(delta);
        BOOST_TEST_EQ( a.load(), T((AddType)value - delta) );
    }

    // Modify and test operations
    test_additive_operators_with_type_and_test< T, D, AddType >();
}

template<typename T, typename D>
void test_additive_operators(T value, D delta)
{
    test_additive_operators_with_type<T, D, T>(value, delta);
}

template< typename T >
void test_negation()
{
    {
        boost::atomic<T> a((T)1);
        T n = a.fetch_negate();
        BOOST_TEST_EQ( a.load(), (T)-1 );
        BOOST_TEST_EQ( n, (T)1 );

        n = a.fetch_negate();
        BOOST_TEST_EQ( a.load(), (T)1 );
        BOOST_TEST_EQ( n, (T)-1 );
    }
    {
        boost::atomic<T> a((T)1);
        T n = a.negate();
        BOOST_TEST_EQ( a.load(), (T)-1 );
        BOOST_TEST_EQ( n, (T)-1 );

        n = a.negate();
        BOOST_TEST_EQ( a.load(), (T)1 );
        BOOST_TEST_EQ( n, (T)1 );
    }
    {
        boost::atomic<T> a((T)1);
        a.opaque_negate();
        BOOST_TEST_EQ( a.load(), (T)-1 );

        a.opaque_negate();
        BOOST_TEST_EQ( a.load(), (T)1 );
    }
    {
        boost::atomic<T> a((T)1);
        bool f = a.negate_and_test();
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), (T)-1 );

        f = a.negate_and_test();
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), (T)1 );
    }
    {
        boost::atomic<T> a((T)0);
        bool f = a.negate_and_test();
        BOOST_TEST_EQ( f, false );
        BOOST_TEST_EQ( a.load(), (T)0 );
    }
}

template<typename T>
void test_additive_wrap(T value)
{
    {
        boost::atomic<T> a(value);
        T n = a.fetch_add(1) + (T)1;
        BOOST_TEST_EQ( a.load(), n );
    }
    {
        boost::atomic<T> a(value);
        T n = a.fetch_sub(1) - (T)1;
        BOOST_TEST_EQ( a.load(), n );
    }
}

template<typename T>
void test_bit_operators(T value, T delta)
{
    /* explicit and/or/xor */
    {
        boost::atomic<T> a(value);
        T n = a.fetch_and(delta);
        BOOST_TEST_EQ( a.load(), T(value & delta) );
        BOOST_TEST_EQ( n, value );
    }

    {
        boost::atomic<T> a(value);
        T n = a.fetch_or(delta);
        BOOST_TEST_EQ( a.load(), T(value | delta) );
        BOOST_TEST_EQ( n, value );
    }

    {
        boost::atomic<T> a(value);
        T n = a.fetch_xor(delta);
        BOOST_TEST_EQ( a.load(), T(value ^ delta) );
        BOOST_TEST_EQ( n, value );
    }

    {
        boost::atomic<T> a(value);
        T n = a.fetch_complement();
        BOOST_TEST_EQ( a.load(), T(~value) );
        BOOST_TEST_EQ( n, value );
    }

    /* overloaded modify/assign */
    {
        boost::atomic<T> a(value);
        T n = (a &= delta);
        BOOST_TEST_EQ( a.load(), T(value & delta) );
        BOOST_TEST_EQ( n, T(value & delta) );
    }

    {
        boost::atomic<T> a(value);
        T n = (a |= delta);
        BOOST_TEST_EQ( a.load(), T(value | delta) );
        BOOST_TEST_EQ( n, T(value | delta) );
    }

    {
        boost::atomic<T> a(value);
        T n = (a ^= delta);
        BOOST_TEST_EQ( a.load(), T(value ^ delta) );
        BOOST_TEST_EQ( n, T(value ^ delta) );
    }

    // Operations returning the actual resulting value
    {
        boost::atomic<T> a(value);
        T n = a.bitwise_and(delta);
        BOOST_TEST_EQ( a.load(), T(value & delta) );
        BOOST_TEST_EQ( n, T(value & delta) );
    }

    {
        boost::atomic<T> a(value);
        T n = a.bitwise_or(delta);
        BOOST_TEST_EQ( a.load(), T(value | delta) );
        BOOST_TEST_EQ( n, T(value | delta) );
    }

    {
        boost::atomic<T> a(value);
        T n = a.bitwise_xor(delta);
        BOOST_TEST_EQ( a.load(), T(value ^ delta) );
        BOOST_TEST_EQ( n, T(value ^ delta) );
    }

    {
        boost::atomic<T> a(value);
        T n = a.bitwise_complement();
        BOOST_TEST_EQ( a.load(), T(~value) );
        BOOST_TEST_EQ( n, T(~value) );
    }

    // Opaque operations
    {
        boost::atomic<T> a(value);
        a.opaque_and(delta);
        BOOST_TEST_EQ( a.load(), T(value & delta) );
    }

    {
        boost::atomic<T> a(value);
        a.opaque_or(delta);
        BOOST_TEST_EQ( a.load(), T(value | delta) );
    }

    {
        boost::atomic<T> a(value);
        a.opaque_xor(delta);
        BOOST_TEST_EQ( a.load(), T(value ^ delta) );
    }

    {
        boost::atomic<T> a(value);
        a.opaque_complement();
        BOOST_TEST_EQ( a.load(), T(~value) );
    }

    // Modify and test operations
    {
        boost::atomic<T> a((T)1);
        bool f = a.and_and_test((T)1);
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), T(1) );

        f = a.and_and_test((T)0);
        BOOST_TEST_EQ( f, false );
        BOOST_TEST_EQ( a.load(), T(0) );

        f = a.and_and_test((T)0);
        BOOST_TEST_EQ( f, false );
        BOOST_TEST_EQ( a.load(), T(0) );
    }

    {
        boost::atomic<T> a((T)0);
        bool f = a.or_and_test((T)0);
        BOOST_TEST_EQ( f, false );
        BOOST_TEST_EQ( a.load(), T(0) );

        f = a.or_and_test((T)1);
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), T(1) );

        f = a.or_and_test((T)1);
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), T(1) );
    }

    {
        boost::atomic<T> a((T)0);
        bool f = a.xor_and_test((T)0);
        BOOST_TEST_EQ( f, false );
        BOOST_TEST_EQ( a.load(), T(0) );

        f = a.xor_and_test((T)1);
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), T(1) );

        f = a.xor_and_test((T)1);
        BOOST_TEST_EQ( f, false );
        BOOST_TEST_EQ( a.load(), T(0) );
    }

    {
        boost::atomic<T> a((T)0);
        bool f = a.complement_and_test();
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), static_cast< T >(~static_cast< T >(0)) );

        f = a.complement_and_test();
        BOOST_TEST_EQ( f, false );
        BOOST_TEST_EQ( a.load(), T(0) );
    }

    // Bit test and modify operations
    {
        boost::atomic<T> a((T)42);
        bool f = a.bit_test_and_set(0);
        BOOST_TEST_EQ( f, false );
        BOOST_TEST_EQ( a.load(), T(43) );

        f = a.bit_test_and_set(1);
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), T(43) );

        f = a.bit_test_and_set(2);
        BOOST_TEST_EQ( f, false );
        BOOST_TEST_EQ( a.load(), T(47) );
    }

    {
        boost::atomic<T> a((T)42);
        bool f = a.bit_test_and_reset(0);
        BOOST_TEST_EQ( f, false );
        BOOST_TEST_EQ( a.load(), T(42) );

        f = a.bit_test_and_reset(1);
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), T(40) );

        f = a.bit_test_and_set(2);
        BOOST_TEST_EQ( f, false );
        BOOST_TEST_EQ( a.load(), T(44) );
    }

    {
        boost::atomic<T> a((T)42);
        bool f = a.bit_test_and_complement(0);
        BOOST_TEST_EQ( f, false );
        BOOST_TEST_EQ( a.load(), T(43) );

        f = a.bit_test_and_complement(1);
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), T(41) );

        f = a.bit_test_and_complement(2);
        BOOST_TEST_EQ( f, false );
        BOOST_TEST_EQ( a.load(), T(45) );
    }
}

template<typename T>
void do_test_integral_api(boost::false_type)
{
    BOOST_TEST(sizeof(boost::atomic<T>) >= sizeof(T));

    test_base_operators<T>(42, 43, 44);
    test_additive_operators<T, T>(42, 17);
    test_bit_operators<T>((T)0x5f5f5f5f5f5f5f5fULL, (T)0xf5f5f5f5f5f5f5f5ULL);

    /* test for unsigned overflow/underflow */
    test_additive_operators<T, T>((T)-1, 1);
    test_additive_operators<T, T>(0, 1);
    /* test for signed overflow/underflow */
    test_additive_operators<T, T>(((T)-1) >> (sizeof(T) * 8 - 1), 1);
    test_additive_operators<T, T>(1 + (((T)-1) >> (sizeof(T) * 8 - 1)), 1);
}

template<typename T>
void do_test_integral_api(boost::true_type)
{
    do_test_integral_api<T>(boost::false_type());

    test_additive_wrap<T>(0u);
    BOOST_CONSTEXPR_OR_CONST T all_ones = ~(T)0u;
    test_additive_wrap<T>(all_ones);
    BOOST_CONSTEXPR_OR_CONST T max_signed_twos_compl = all_ones >> 1;
    test_additive_wrap<T>(all_ones ^ max_signed_twos_compl);
    test_additive_wrap<T>(max_signed_twos_compl);
}

template<typename T>
inline void test_integral_api(void)
{
    do_test_integral_api<T>(boost::is_unsigned<T>());

    if (boost::is_signed<T>::value)
        test_negation<T>();
}

#if !defined(BOOST_ATOMIC_NO_FLOATING_POINT)

template<typename T, typename D>
void test_fp_additive_operators(T value, D delta)
{
    /* explicit add/sub */
    {
        boost::atomic<T> a(value);
        T n = a.fetch_add(delta);
        BOOST_TEST_EQ( a.load(), approx(T(value + delta)) );
        BOOST_TEST_EQ( n, approx(value) );
    }

    {
        boost::atomic<T> a(value);
        T n = a.fetch_sub(delta);
        BOOST_TEST_EQ( a.load(), approx(T(value - delta)) );
        BOOST_TEST_EQ( n, approx(value) );
    }

    /* overloaded modify/assign*/
    {
        boost::atomic<T> a(value);
        T n = (a += delta);
        BOOST_TEST_EQ( a.load(), approx(T(value + delta)) );
        BOOST_TEST_EQ( n, approx(T(value + delta)) );
    }

    {
        boost::atomic<T> a(value);
        T n = (a -= delta);
        BOOST_TEST_EQ( a.load(), approx(T(value - delta)) );
        BOOST_TEST_EQ( n, approx(T(value - delta)) );
    }

    // Operations returning the actual resulting value
    {
        boost::atomic<T> a(value);
        T n = a.add(delta);
        BOOST_TEST_EQ( a.load(), approx(T(value + delta)) );
        BOOST_TEST_EQ( n, approx(T(value + delta)) );
    }

    {
        boost::atomic<T> a(value);
        T n = a.sub(delta);
        BOOST_TEST_EQ( a.load(), approx(T(value - delta)) );
        BOOST_TEST_EQ( n, approx(T(value - delta)) );
    }

    // Opaque operations
    {
        boost::atomic<T> a(value);
        a.opaque_add(delta);
        BOOST_TEST_EQ( a.load(), approx(T(value + delta)) );
    }

    {
        boost::atomic<T> a(value);
        a.opaque_sub(delta);
        BOOST_TEST_EQ( a.load(), approx(T(value - delta)) );
    }
}

template< typename T >
void test_fp_negation()
{
    {
        boost::atomic<T> a((T)1);
        T n = a.fetch_negate();
        BOOST_TEST_EQ( a.load(), approx((T)-1) );
        BOOST_TEST_EQ( n, approx((T)1) );

        n = a.fetch_negate();
        BOOST_TEST_EQ( a.load(), approx((T)1) );
        BOOST_TEST_EQ( n, approx((T)-1) );
    }
    {
        boost::atomic<T> a((T)1);
        T n = a.negate();
        BOOST_TEST_EQ( a.load(), approx((T)-1) );
        BOOST_TEST_EQ( n, approx((T)-1) );

        n = a.negate();
        BOOST_TEST_EQ( a.load(), approx((T)1) );
        BOOST_TEST_EQ( n, approx((T)1) );
    }
    {
        boost::atomic<T> a((T)1);
        a.opaque_negate();
        BOOST_TEST_EQ( a.load(), approx((T)-1) );

        a.opaque_negate();
        BOOST_TEST_EQ( a.load(), approx((T)1) );
    }
}

#endif // !defined(BOOST_ATOMIC_NO_FLOATING_POINT)

template<typename T>
inline void test_floating_point_api(void)
{
    BOOST_TEST(sizeof(boost::atomic<T>) >= sizeof(T));

    // Note: When support for floating point is disabled, even the base operation tests may fail because
    // the generic template specialization does not account for garbage in padding bits that are present in some FP types.
#if !defined(BOOST_ATOMIC_NO_FLOATING_POINT)
    test_base_operators<T>(static_cast<T>(42.1), static_cast<T>(43.2), static_cast<T>(44.3));

    test_fp_additive_operators<T, T>(static_cast<T>(42.5), static_cast<T>(17.7));
    test_fp_additive_operators<T, T>(static_cast<T>(-42.5), static_cast<T>(-17.7));

    test_fp_negation<T>();
#endif
}


template<typename T>
void test_pointer_api(void)
{
    BOOST_TEST_GE( sizeof(boost::atomic<T *>), sizeof(T *));
    BOOST_TEST_GE( sizeof(boost::atomic<void *>), sizeof(T *));

    T values[3];

    test_base_operators<T*>(&values[0], &values[1], &values[2]);
    test_additive_operators<T*>(&values[1], 1);

    test_base_operators<void*>(&values[0], &values[1], &values[2]);

#if defined(BOOST_HAS_INTPTR_T)
    boost::atomic<void *> ptr;
    boost::atomic<boost::intptr_t> integral;
    BOOST_TEST_EQ( ptr.is_lock_free(), integral.is_lock_free() );
#endif
}

enum test_enum
{
    foo, bar, baz
};

static void
test_enum_api(void)
{
    test_base_operators(foo, bar, baz);
}

template<typename T>
struct test_struct
{
    typedef T value_type;
    value_type i;
    inline bool operator==(const test_struct & c) const {return i == c.i;}
    inline bool operator!=(const test_struct & c) const {return i != c.i;}
};

template< typename Char, typename Traits, typename T >
inline std::basic_ostream< Char, Traits >& operator<< (std::basic_ostream< Char, Traits >& strm, test_struct< T > const& s)
{
    test_stream << "{" << s.i << "}";
    return strm;
}

template<typename T>
void
test_struct_api(void)
{
    T a = {1}, b = {2}, c = {3};

    test_base_operators(a, b, c);

    {
        boost::atomic<T> sa;
        boost::atomic<typename T::value_type> si;
        BOOST_TEST_EQ( sa.is_lock_free(), si.is_lock_free() );
    }
}

template<typename T>
struct test_struct_x2
{
    typedef T value_type;
    value_type i, j;
    inline bool operator==(const test_struct_x2 & c) const {return i == c.i && j == c.j;}
    inline bool operator!=(const test_struct_x2 & c) const {return i != c.i && j != c.j;}
};

template< typename Char, typename Traits, typename T >
inline std::basic_ostream< Char, Traits >& operator<< (std::basic_ostream< Char, Traits >& strm, test_struct_x2< T > const& s)
{
    test_stream << "{" << s.i << ", " << s.j << "}";
    return strm;
}

template<typename T>
void
test_struct_x2_api(void)
{
    T a = {1, 1}, b = {2, 2}, c = {3, 3};

    test_base_operators(a, b, c);
}

struct large_struct
{
    long data[64];

    inline bool operator==(const large_struct & c) const
    {
        return std::memcmp(data, &c.data, sizeof(data)) == 0;
    }
    inline bool operator!=(const large_struct & c) const
    {
        return std::memcmp(data, &c.data, sizeof(data)) != 0;
    }
};

template< typename Char, typename Traits >
inline std::basic_ostream< Char, Traits >& operator<< (std::basic_ostream< Char, Traits >& strm, large_struct const&)
{
    strm << "[large_struct]";
    return strm;
}

static void
test_large_struct_api(void)
{
    large_struct a = {{1}}, b = {{2}}, c = {{3}};
    test_base_operators(a, b, c);
}

struct test_struct_with_ctor
{
    typedef unsigned int value_type;
    value_type i;
    test_struct_with_ctor() : i(0x01234567) {}
    inline bool operator==(const test_struct_with_ctor & c) const {return i == c.i;}
    inline bool operator!=(const test_struct_with_ctor & c) const {return i != c.i;}
};

template< typename Char, typename Traits >
inline std::basic_ostream< Char, Traits >& operator<< (std::basic_ostream< Char, Traits >& strm, test_struct_with_ctor const&)
{
    strm << "[test_struct_with_ctor]";
    return strm;
}

static void
test_struct_with_ctor_api(void)
{
    {
        test_struct_with_ctor s;
        boost::atomic<test_struct_with_ctor> sa;
        // Check that the default constructor was called
        BOOST_TEST( sa.load() == s );
    }

    test_struct_with_ctor a, b, c;
    a.i = 1;
    b.i = 2;
    c.i = 3;

    test_base_operators(a, b, c);
}

#endif
