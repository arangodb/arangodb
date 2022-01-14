//  Copyright (c) 2011 Helge Bahmann
//  Copyright (c) 2017 - 2020 Andrey Semashev
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_ATOMIC_API_TEST_HELPERS_HPP
#define BOOST_ATOMIC_API_TEST_HELPERS_HPP

#include <boost/atomic.hpp>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <vector>
#include <iostream>
#include <boost/config.hpp>
#include <boost/cstdint.hpp>
#include <boost/type.hpp>
#include <boost/core/enable_if.hpp>
#include <boost/type_traits/integral_constant.hpp>
#include <boost/type_traits/alignment_of.hpp>
#include <boost/type_traits/is_pointer.hpp>
#include <boost/type_traits/is_signed.hpp>
#include <boost/type_traits/is_unsigned.hpp>
#include <boost/type_traits/make_signed.hpp>
#include <boost/type_traits/make_unsigned.hpp>
#include <boost/type_traits/conditional.hpp>

#include "lightweight_test_stream.hpp"
#include "value_with_epsilon.hpp"
#include "atomic_wrapper.hpp"

const unsigned int max_weak_cas_loops = 1000;

template< typename T >
struct is_atomic :
    public boost::false_type
{
};

template< typename T >
struct is_atomic< boost::atomic< T > > :
    public boost::true_type
{
};

template< typename T >
struct is_atomic< boost::ipc_atomic< T > > :
    public boost::true_type
{
};

template< typename T >
struct is_atomic_ref :
    public boost::false_type
{
};

template< typename T >
struct is_atomic_ref< boost::atomic_ref< T > > :
    public boost::true_type
{
};

template< typename T >
struct is_atomic_ref< boost::ipc_atomic_ref< T > > :
    public boost::true_type
{
};

template< typename Flag >
inline void test_flag_api(void)
{
#ifndef BOOST_ATOMIC_NO_ATOMIC_FLAG_INIT
    Flag f = BOOST_ATOMIC_FLAG_INIT;
#else
    Flag f;
#endif

    BOOST_TEST( !f.test() );
    BOOST_TEST( !f.test_and_set() );
    BOOST_TEST( f.test() );
    BOOST_TEST( f.test_and_set() );
    BOOST_TEST( f.test() );
    f.clear();
    BOOST_TEST( !f.test() );
    BOOST_TEST( !f.test_and_set() );
}

template< typename T >
inline typename boost::enable_if< is_atomic< T > >::type test_atomic_type_traits(boost::type< T >)
{
    BOOST_TEST_GE(sizeof(T), sizeof(typename T::value_type));
}

template< typename T >
inline typename boost::enable_if< is_atomic_ref< T > >::type test_atomic_type_traits(boost::type< T >)
{
    if (T::is_always_lock_free)
    {
        BOOST_TEST_GE(T::required_alignment, boost::alignment_of< typename T::value_type >::value);
    }
    else
    {
        // Lock-based implementation should not require alignment higher than alignof(T)
        BOOST_TEST_EQ(T::required_alignment, boost::alignment_of< typename T::value_type >::value);
    }
}

template< template< typename > class Wrapper, typename T >
void test_base_operators(T value1, T value2, T value3)
{
    test_atomic_type_traits(boost::type< typename Wrapper<T>::atomic_type >());

    // explicit load/store
    {
        Wrapper<T> wrapper(value1);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        BOOST_TEST_EQ( a.load(), value1 );
    }

    {
        Wrapper<T> wrapper(value1);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.store(value2);
        BOOST_TEST_EQ( a.load(), value2 );
    }

    // overloaded assignment/conversion
    {
        Wrapper<T> wrapper(value1);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        BOOST_TEST( value1 == a );
    }

    {
        Wrapper<T> wrapper(value1);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a = value2;
        BOOST_TEST( value2 == a );
    }

    // exchange-type operators
    {
        Wrapper<T> wrapper(value1);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.exchange(value2);
        BOOST_TEST_EQ( a.load(), value2 );
        BOOST_TEST_EQ( n, value1 );
    }

    {
        Wrapper<T> wrapper(value1);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T expected = value1;
        bool success = a.compare_exchange_strong(expected, value3);
        BOOST_TEST( success );
        BOOST_TEST_EQ( a.load(), value3 );
        BOOST_TEST_EQ( expected, value1 );
    }

    {
        Wrapper<T> wrapper(value1);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T expected = value2;
        bool success = a.compare_exchange_strong(expected, value3);
        BOOST_TEST( !success );
        BOOST_TEST_EQ( a.load(), value1 );
        BOOST_TEST_EQ( expected, value1 );
    }

    {
        Wrapper<T> wrapper(value1);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T expected;
        unsigned int loops = 0;
        bool success = false;
        do
        {
            expected = value1;
            success = a.compare_exchange_weak(expected, value3);
            ++loops;
        }
        while (!success && loops < max_weak_cas_loops);
        BOOST_TEST( success );
        BOOST_TEST_EQ( a.load(), value3 );
        BOOST_TEST_EQ( expected, value1 );
    }

    {
        Wrapper<T> wrapper(value1);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T expected;
        unsigned int loops = 0;
        bool success = false;
        do
        {
            expected = value2;
            success = a.compare_exchange_weak(expected, value3);
            if (expected != value2)
                break;
            ++loops;
        }
        while (!success && loops < max_weak_cas_loops);
        BOOST_TEST( !success );
        BOOST_TEST_EQ( a.load(), value1 );
        BOOST_TEST_EQ( expected, value1 );
    }
}

//! Tests whether boost::atomic supports constexpr constructor. Note that boost::atomic_ref (as std::atomic_ref) does not support constexpr constructor.
template< typename T >
void test_constexpr_ctor()
{
#ifndef BOOST_ATOMIC_DETAIL_NO_CXX11_CONSTEXPR_UNION_INIT
    constexpr T value(0);
    constexpr boost::atomic<T> tester(value);
    BOOST_TEST( tester == value );
#endif
}

//! The type traits provides max and min values of type D that can be added/subtracted to T(0) without signed overflow
template< typename T, typename D, bool IsSigned = boost::is_signed< D >::value >
struct distance_limits
{
    //! Difference type D promoted to the width of type T
    typedef typename boost::conditional<
        IsSigned,
        boost::make_signed< T >,
        boost::make_unsigned< T >
    >::type::type promoted_difference_type;

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
    //! Difference type D promoted to the width of type T
    typedef std::ptrdiff_t promoted_difference_type;

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
    //! Difference type D promoted to the width of type T
    typedef std::size_t promoted_difference_type;

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
    //! Difference type D promoted to the width of type T
    typedef boost::int128_type promoted_difference_type;

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
    //! Difference type D promoted to the width of type T
    typedef boost::uint128_type promoted_difference_type;

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

#if defined(BOOST_MSVC)
#pragma warning(push)
// unary minus operator applied to unsigned type, result still unsigned
#pragma warning(disable: 4146)
#endif

template< template< typename > class Wrapper, typename T, typename D, typename AddType >
void test_additive_operators_with_type_and_test()
{
#if defined(UBSAN)
    // clang UBSAN flags this test when AddType is a pointer as it considers subtracting from a null pointer (zero_add) an UB
    if (boost::is_pointer< AddType >::value)
        return;
#endif

    // Note: This set of tests is extracted to a separate function because otherwise MSVC-10 for x64 generates broken code
    typedef typename distance_limits< T, D >::promoted_difference_type promoted_difference_type;
    typedef typename boost::make_unsigned< promoted_difference_type >::type unsigned_promoted_difference_type;
    const T zero_value = 0;
    const D zero_diff = 0;
    const D one_diff = 1;
    const AddType zero_add = 0;
    {
        Wrapper<T> wrapper(zero_value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        bool f = a.add_and_test(zero_diff);
        BOOST_TEST_EQ( f, false );
        BOOST_TEST_EQ( a.load(), zero_value );

        f = a.add_and_test(one_diff);
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), T(zero_add + one_diff) );
    }
    {
        Wrapper<T> wrapper(zero_value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        bool f = a.add_and_test((distance_limits< T, D >::max)());
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), T(zero_add + (distance_limits< T, D >::max)()) );
    }
    {
        Wrapper<T> wrapper(zero_value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        bool f = a.add_and_test((distance_limits< T, D >::min)());
        BOOST_TEST_EQ( f, ((distance_limits< T, D >::min)() != 0) );
        BOOST_TEST_EQ( a.load(), T(zero_add + (distance_limits< T, D >::min)()) );
    }

    {
        Wrapper<T> wrapper(zero_value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        bool f = a.sub_and_test(zero_diff);
        BOOST_TEST_EQ( f, false );
        BOOST_TEST_EQ( a.load(), zero_value );

        f = a.sub_and_test(one_diff);
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), T(zero_add - one_diff) );
    }
    {
        Wrapper<T> wrapper(zero_value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        bool f = a.sub_and_test((distance_limits< T, D >::max)());
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), T(zero_add - (distance_limits< T, D >::max)()) );
    }
    {
        Wrapper<T> wrapper(zero_value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        bool f = a.sub_and_test((distance_limits< T, D >::min)());
        BOOST_TEST_EQ( f, ((distance_limits< T, D >::min)() != 0) );
        // Be very careful as to not cause signed overflow on negation
        unsigned_promoted_difference_type umin = static_cast< unsigned_promoted_difference_type >(
            static_cast< promoted_difference_type >((distance_limits< T, D >::min)()));
        umin = -umin;
        promoted_difference_type neg_min;
        std::memcpy(&neg_min, &umin, sizeof(neg_min));
        BOOST_TEST_EQ( a.load(), T(zero_add + neg_min) );
    }
}

#if defined(BOOST_MSVC)
#pragma warning(pop)
#endif

template< template< typename > class Wrapper, typename T, typename D, typename AddType >
void test_additive_operators_with_type(T value, D delta)
{
    /* note: the tests explicitly cast the result of any addition
    to the type to be tested to force truncation of the result to
    the correct range in case of overflow */

    // explicit add/sub
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_add(delta);
        BOOST_TEST_EQ( a.load(), T((AddType)value + delta) );
        BOOST_TEST_EQ( n, value );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_sub(delta);
        BOOST_TEST_EQ( a.load(), T((AddType)value - delta) );
        BOOST_TEST_EQ( n, value );
    }

    // add/sub with an immediate
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_add(1);
        BOOST_TEST_EQ( a.load(), T((AddType)value + 1) );
        BOOST_TEST_EQ( n, value );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_sub(1);
        BOOST_TEST_EQ( a.load(), T((AddType)value - 1) );
        BOOST_TEST_EQ( n, value );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_add(76);
        BOOST_TEST_EQ( a.load(), T((AddType)value + 76) );
        BOOST_TEST_EQ( n, value );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_sub(76);
        BOOST_TEST_EQ( a.load(), T((AddType)value - 76) );
        BOOST_TEST_EQ( n, value );
    }

    if ((std::numeric_limits< D >::max)() >= 4097)
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_add((D)4097);
        BOOST_TEST_EQ( a.load(), T((AddType)value + (D)4097) );
        BOOST_TEST_EQ( n, value );
    }

    if ((std::numeric_limits< D >::max)() >= 4097)
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_sub((D)4097);
        BOOST_TEST_EQ( a.load(), T((AddType)value - (D)4097) );
        BOOST_TEST_EQ( n, value );
    }

    // overloaded modify/assign
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = (a += delta);
        BOOST_TEST_EQ( a.load(), T((AddType)value + delta) );
        BOOST_TEST_EQ( n, T((AddType)value + delta) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = (a -= delta);
        BOOST_TEST_EQ( a.load(), T((AddType)value - delta) );
        BOOST_TEST_EQ( n, T((AddType)value - delta) );
    }

    // overloaded increment/decrement
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a++;
        BOOST_TEST_EQ( a.load(), T((AddType)value + 1) );
        BOOST_TEST_EQ( n, value );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = ++a;
        BOOST_TEST_EQ( a.load(), T((AddType)value + 1) );
        BOOST_TEST_EQ( n, T((AddType)value + 1) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a--;
        BOOST_TEST_EQ( a.load(), T((AddType)value - 1) );
        BOOST_TEST_EQ( n, value );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = --a;
        BOOST_TEST_EQ( a.load(), T((AddType)value - 1) );
        BOOST_TEST_EQ( n, T((AddType)value - 1) );
    }

    // Operations returning the actual resulting value
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.add(delta);
        BOOST_TEST_EQ( a.load(), T((AddType)value + delta) );
        BOOST_TEST_EQ( n, T((AddType)value + delta) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.sub(delta);
        BOOST_TEST_EQ( a.load(), T((AddType)value - delta) );
        BOOST_TEST_EQ( n, T((AddType)value - delta) );
    }

    // The same with an immediate
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.add(1);
        BOOST_TEST_EQ( a.load(), T((AddType)value + 1) );
        BOOST_TEST_EQ( n, T((AddType)value + 1) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.sub(1);
        BOOST_TEST_EQ( a.load(), T((AddType)value - 1) );
        BOOST_TEST_EQ( n, T((AddType)value - 1) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.add(76);
        BOOST_TEST_EQ( a.load(), T((AddType)value + 76) );
        BOOST_TEST_EQ( n, T((AddType)value + 76) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.sub(76);
        BOOST_TEST_EQ( a.load(), T((AddType)value - 76) );
        BOOST_TEST_EQ( n, T((AddType)value - 76) );
    }

    if ((std::numeric_limits< D >::max)() >= 4097)
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.add((D)4097);
        BOOST_TEST_EQ( a.load(), T((AddType)value + (D)4097) );
        BOOST_TEST_EQ( n, T((AddType)value + (D)4097) );
    }

    if ((std::numeric_limits< D >::max)() >= 4097)
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.sub((D)4097);
        BOOST_TEST_EQ( a.load(), T((AddType)value - (D)4097) );
        BOOST_TEST_EQ( n, T((AddType)value - (D)4097) );
    }

    // Opaque operations
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_add(delta);
        BOOST_TEST_EQ( a.load(), T((AddType)value + delta) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_sub(delta);
        BOOST_TEST_EQ( a.load(), T((AddType)value - delta) );
    }

    // The same with an immediate
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_add(1);
        BOOST_TEST_EQ( a.load(), T((AddType)value + 1) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_sub(1);
        BOOST_TEST_EQ( a.load(), T((AddType)value - 1) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_add(76);
        BOOST_TEST_EQ( a.load(), T((AddType)value + 76) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_sub(76);
        BOOST_TEST_EQ( a.load(), T((AddType)value - 76) );
    }

    if ((std::numeric_limits< D >::max)() >= 4097)
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_add((D)4097);
        BOOST_TEST_EQ( a.load(), T((AddType)value + (D)4097) );
    }

    if ((std::numeric_limits< D >::max)() >= 4097)
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_sub((D)4097);
        BOOST_TEST_EQ( a.load(), T((AddType)value - (D)4097) );
    }

    // Modify and test operations
    test_additive_operators_with_type_and_test< Wrapper, T, D, AddType >();
}

template< template< typename > class Wrapper, typename T, typename D >
void test_additive_operators(T value, D delta)
{
    test_additive_operators_with_type< Wrapper, T, D, T >(value, delta);
}

template< template< typename > class Wrapper, typename T >
void test_negation()
{
    {
        Wrapper<T> wrapper((T)1);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_negate();
        BOOST_TEST_EQ( a.load(), (T)-1 );
        BOOST_TEST_EQ( n, (T)1 );

        n = a.fetch_negate();
        BOOST_TEST_EQ( a.load(), (T)1 );
        BOOST_TEST_EQ( n, (T)-1 );
    }
    {
        Wrapper<T> wrapper((T)1);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.negate();
        BOOST_TEST_EQ( a.load(), (T)-1 );
        BOOST_TEST_EQ( n, (T)-1 );

        n = a.negate();
        BOOST_TEST_EQ( a.load(), (T)1 );
        BOOST_TEST_EQ( n, (T)1 );
    }
    {
        Wrapper<T> wrapper((T)1);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_negate();
        BOOST_TEST_EQ( a.load(), (T)-1 );

        a.opaque_negate();
        BOOST_TEST_EQ( a.load(), (T)1 );
    }
    {
        Wrapper<T> wrapper((T)1);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        bool f = a.negate_and_test();
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), (T)-1 );

        f = a.negate_and_test();
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), (T)1 );
    }
    {
        Wrapper<T> wrapper((T)0);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        bool f = a.negate_and_test();
        BOOST_TEST_EQ( f, false );
        BOOST_TEST_EQ( a.load(), (T)0 );
    }
}

template< template< typename > class Wrapper, typename T >
void test_additive_wrap(T value)
{
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_add(1) + (T)1;
        BOOST_TEST_EQ( a.load(), n );
    }
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_sub(1) - (T)1;
        BOOST_TEST_EQ( a.load(), n );
    }
}

template< template< typename > class Wrapper, typename T >
void test_bit_operators(T value, T delta)
{
    // explicit and/or/xor
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_and(delta);
        BOOST_TEST_EQ( a.load(), T(value & delta) );
        BOOST_TEST_EQ( n, value );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_or(delta);
        BOOST_TEST_EQ( a.load(), T(value | delta) );
        BOOST_TEST_EQ( n, value );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_xor(delta);
        BOOST_TEST_EQ( a.load(), T(value ^ delta) );
        BOOST_TEST_EQ( n, value );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_complement();
        BOOST_TEST_EQ( a.load(), T(~value) );
        BOOST_TEST_EQ( n, value );
    }

    // and/or/xor with an immediate. The immediates below are chosen to either be encodable in an instruction or not for various target architectures.
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_and((T)1);
        BOOST_TEST_EQ( a.load(), T(value & (T)1) );
        BOOST_TEST_EQ( n, value );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_or((T)1);
        BOOST_TEST_EQ( a.load(), T(value | (T)1) );
        BOOST_TEST_EQ( n, value );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_xor((T)1);
        BOOST_TEST_EQ( a.load(), T(value ^ (T)1) );
        BOOST_TEST_EQ( n, value );
    }

    // The following constants are not encodable in AArch64
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_and((T)76);
        BOOST_TEST_EQ( a.load(), T(value & (T)76) );
        BOOST_TEST_EQ( n, value );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_or((T)76);
        BOOST_TEST_EQ( a.load(), T(value | (T)76) );
        BOOST_TEST_EQ( n, value );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_xor((T)76);
        BOOST_TEST_EQ( a.load(), T(value ^ (T)76) );
        BOOST_TEST_EQ( n, value );
    }

    // overloaded modify/assign
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = (a &= delta);
        BOOST_TEST_EQ( a.load(), T(value & delta) );
        BOOST_TEST_EQ( n, T(value & delta) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = (a |= delta);
        BOOST_TEST_EQ( a.load(), T(value | delta) );
        BOOST_TEST_EQ( n, T(value | delta) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = (a ^= delta);
        BOOST_TEST_EQ( a.load(), T(value ^ delta) );
        BOOST_TEST_EQ( n, T(value ^ delta) );
    }

    // Operations returning the actual resulting value
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.bitwise_and(delta);
        BOOST_TEST_EQ( a.load(), T(value & delta) );
        BOOST_TEST_EQ( n, T(value & delta) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.bitwise_or(delta);
        BOOST_TEST_EQ( a.load(), T(value | delta) );
        BOOST_TEST_EQ( n, T(value | delta) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.bitwise_xor(delta);
        BOOST_TEST_EQ( a.load(), T(value ^ delta) );
        BOOST_TEST_EQ( n, T(value ^ delta) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.bitwise_complement();
        BOOST_TEST_EQ( a.load(), T(~value) );
        BOOST_TEST_EQ( n, T(~value) );
    }

    // The same with an immediate
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.bitwise_and((T)1);
        BOOST_TEST_EQ( a.load(), T(value & (T)1) );
        BOOST_TEST_EQ( n, T(value & (T)1) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.bitwise_or((T)1);
        BOOST_TEST_EQ( a.load(), T(value | (T)1) );
        BOOST_TEST_EQ( n, T(value | (T)1) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.bitwise_xor((T)1);
        BOOST_TEST_EQ( a.load(), T(value ^ (T)1) );
        BOOST_TEST_EQ( n, T(value ^ (T)1) );
    }

    // The following constants are not encodable in AArch64
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.bitwise_and((T)76);
        BOOST_TEST_EQ( a.load(), T(value & (T)76) );
        BOOST_TEST_EQ( n, T(value & (T)76) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.bitwise_or((T)76);
        BOOST_TEST_EQ( a.load(), T(value | (T)76) );
        BOOST_TEST_EQ( n, T(value | (T)76) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.bitwise_xor((T)76);
        BOOST_TEST_EQ( a.load(), T(value ^ (T)76) );
        BOOST_TEST_EQ( n, T(value ^ (T)76) );
    }

    // Opaque operations
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_and(delta);
        BOOST_TEST_EQ( a.load(), T(value & delta) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_or(delta);
        BOOST_TEST_EQ( a.load(), T(value | delta) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_xor(delta);
        BOOST_TEST_EQ( a.load(), T(value ^ delta) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_complement();
        BOOST_TEST_EQ( a.load(), T(~value) );
    }

    // The same with an immediate
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_and((T)1);
        BOOST_TEST_EQ( a.load(), T(value & (T)1) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_or((T)1);
        BOOST_TEST_EQ( a.load(), T(value | (T)1) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_xor((T)1);
        BOOST_TEST_EQ( a.load(), T(value ^ (T)1) );
    }

    // The following constants are not encodable in AArch64
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_and((T)76);
        BOOST_TEST_EQ( a.load(), T(value & (T)76) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_or((T)76);
        BOOST_TEST_EQ( a.load(), T(value | (T)76) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_xor((T)76);
        BOOST_TEST_EQ( a.load(), T(value ^ (T)76) );
    }

    // Modify and test operations
    {
        Wrapper<T> wrapper((T)1);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
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
        Wrapper<T> wrapper((T)0);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
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
        Wrapper<T> wrapper((T)0);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
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
        Wrapper<T> wrapper((T)0);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        bool f = a.complement_and_test();
        BOOST_TEST_EQ( f, true );
        BOOST_TEST_EQ( a.load(), static_cast< T >(~static_cast< T >(0)) );

        f = a.complement_and_test();
        BOOST_TEST_EQ( f, false );
        BOOST_TEST_EQ( a.load(), T(0) );
    }

    // Bit test and modify operations
    {
        Wrapper<T> wrapper((T)42);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
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
        Wrapper<T> wrapper((T)42);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
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
        Wrapper<T> wrapper((T)42);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
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

    // Test that a runtime value works for the bit index. This is important for asm block constraints.
    {
        unsigned int runtime_bit_index = std::rand() & 7u;
        Wrapper<T> wrapper((T)42);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;

        a.bit_test_and_set(runtime_bit_index);
        a.bit_test_and_reset(runtime_bit_index);
        a.bit_test_and_complement(runtime_bit_index);
    }
}

template< template< typename > class Wrapper, typename T >
void do_test_integral_api(boost::false_type)
{
    test_base_operators< Wrapper, T >(42, 43, 44);
    test_additive_operators< Wrapper, T, T >(42, 17);
    test_bit_operators< Wrapper, T >((T)0x5f5f5f5f5f5f5f5fULL, (T)0xf5f5f5f5f5f5f5f5ULL);

    /* test for unsigned overflow/underflow */
    test_additive_operators< Wrapper, T, T >((T)-1, 1);
    test_additive_operators< Wrapper, T, T >(0, 1);
    /* test for signed overflow/underflow */
    test_additive_operators< Wrapper, T, T >(((T)-1) >> (sizeof(T) * 8 - 1), 1);
    test_additive_operators< Wrapper, T, T >(1 + (((T)-1) >> (sizeof(T) * 8 - 1)), 1);
}

template< template< typename > class Wrapper, typename T >
void do_test_integral_api(boost::true_type)
{
    do_test_integral_api< Wrapper, T >(boost::false_type());

    test_additive_wrap< Wrapper, T >(0u);
    BOOST_CONSTEXPR_OR_CONST T all_ones = ~(T)0u;
    test_additive_wrap< Wrapper, T >(all_ones);
    BOOST_CONSTEXPR_OR_CONST T max_signed_twos_compl = all_ones >> 1;
    test_additive_wrap< Wrapper, T >(all_ones ^ max_signed_twos_compl);
    test_additive_wrap< Wrapper, T >(max_signed_twos_compl);
}

template< template< typename > class Wrapper, typename T >
inline void test_integral_api(void)
{
    do_test_integral_api< Wrapper, T >(boost::is_unsigned<T>());

    if (boost::is_signed<T>::value)
        test_negation< Wrapper, T >();
}

template< template< typename > class Wrapper, typename T >
inline void test_lock_free_integral_api(boost::true_type)
{
    test_integral_api< Wrapper, T >();
}

template< template< typename > class Wrapper, typename T >
inline void test_lock_free_integral_api(boost::false_type)
{
}

template< template< typename > class Wrapper, typename T >
inline void test_lock_free_integral_api(void)
{
    test_lock_free_integral_api< Wrapper, T >(boost::integral_constant< bool, Wrapper< T >::atomic_type::is_always_lock_free >());
}

#if !defined(BOOST_ATOMIC_NO_FLOATING_POINT)

template< template< typename > class Wrapper, typename T, typename D >
void test_fp_additive_operators(T value, D delta)
{
    // explicit add/sub
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_add(delta);
        BOOST_TEST_EQ( a.load(), approx(T(value + delta)) );
        BOOST_TEST_EQ( n, approx(value) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_sub(delta);
        BOOST_TEST_EQ( a.load(), approx(T(value - delta)) );
        BOOST_TEST_EQ( n, approx(value) );
    }

    // overloaded modify/assign
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = (a += delta);
        BOOST_TEST_EQ( a.load(), approx(T(value + delta)) );
        BOOST_TEST_EQ( n, approx(T(value + delta)) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = (a -= delta);
        BOOST_TEST_EQ( a.load(), approx(T(value - delta)) );
        BOOST_TEST_EQ( n, approx(T(value - delta)) );
    }

    // Operations returning the actual resulting value
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.add(delta);
        BOOST_TEST_EQ( a.load(), approx(T(value + delta)) );
        BOOST_TEST_EQ( n, approx(T(value + delta)) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.sub(delta);
        BOOST_TEST_EQ( a.load(), approx(T(value - delta)) );
        BOOST_TEST_EQ( n, approx(T(value - delta)) );
    }

    // Opaque operations
    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_add(delta);
        BOOST_TEST_EQ( a.load(), approx(T(value + delta)) );
    }

    {
        Wrapper<T> wrapper(value);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_sub(delta);
        BOOST_TEST_EQ( a.load(), approx(T(value - delta)) );
    }
}

template< template< typename > class Wrapper, typename T >
void test_fp_negation()
{
    {
        Wrapper<T> wrapper((T)1);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.fetch_negate();
        BOOST_TEST_EQ( a.load(), approx((T)-1) );
        BOOST_TEST_EQ( n, approx((T)1) );

        n = a.fetch_negate();
        BOOST_TEST_EQ( a.load(), approx((T)1) );
        BOOST_TEST_EQ( n, approx((T)-1) );
    }
    {
        Wrapper<T> wrapper((T)1);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        T n = a.negate();
        BOOST_TEST_EQ( a.load(), approx((T)-1) );
        BOOST_TEST_EQ( n, approx((T)-1) );

        n = a.negate();
        BOOST_TEST_EQ( a.load(), approx((T)1) );
        BOOST_TEST_EQ( n, approx((T)1) );
    }
    {
        Wrapper<T> wrapper((T)1);
        typename Wrapper<T>::atomic_reference_type a = wrapper.a;
        a.opaque_negate();
        BOOST_TEST_EQ( a.load(), approx((T)-1) );

        a.opaque_negate();
        BOOST_TEST_EQ( a.load(), approx((T)1) );
    }
}

#endif // !defined(BOOST_ATOMIC_NO_FLOATING_POINT)

template< template< typename > class Wrapper, typename T >
void test_floating_point_api(void)
{
    // Note: When support for floating point is disabled, even the base operation tests may fail because
    // the generic template specialization does not account for garbage in padding bits that are present in some FP types.
#if !defined(BOOST_ATOMIC_NO_FLOATING_POINT)
    test_base_operators< Wrapper, T >(static_cast<T>(42.1), static_cast<T>(43.2), static_cast<T>(44.3));

    test_fp_additive_operators< Wrapper, T, T >(static_cast<T>(42.5), static_cast<T>(17.7));
    test_fp_additive_operators< Wrapper, T, T >(static_cast<T>(-42.5), static_cast<T>(-17.7));

    test_fp_negation< Wrapper, T >();
#endif
}

template< template< typename > class Wrapper, typename T >
inline void test_lock_free_floating_point_api(boost::true_type)
{
    test_floating_point_api< Wrapper, T >();
}

template< template< typename > class Wrapper, typename T >
inline void test_lock_free_floating_point_api(boost::false_type)
{
}

template< template< typename > class Wrapper, typename T >
inline void test_lock_free_floating_point_api(void)
{
    test_lock_free_floating_point_api< Wrapper, T >(boost::integral_constant< bool, Wrapper< T >::atomic_type::is_always_lock_free >());
}


template< template< typename > class Wrapper, typename T >
void test_pointer_api(void)
{
    std::vector< T > values;
    values.resize(5000); // make the vector large enough to accommodate pointer arithmetics in the additive tests

    test_base_operators< Wrapper, T* >(&values[0], &values[1], &values[2]);
    test_additive_operators< Wrapper, T* >(&values[1], 1);

    test_base_operators< Wrapper, void* >(&values[0], &values[1], &values[2]);

#if defined(BOOST_HAS_INTPTR_T)
    Wrapper<void*> wrapper_ptr;
    typename Wrapper<void*>::atomic_reference_type ptr = wrapper_ptr.a;
    Wrapper<boost::intptr_t> wrapper_integral;
    typename Wrapper<boost::intptr_t>::atomic_reference_type integral = wrapper_integral.a;
    BOOST_TEST_EQ( ptr.is_lock_free(), integral.is_lock_free() );
#endif
}

enum test_enum
{
    foo, bar, baz
};

template< template< typename > class Wrapper, typename T >
inline void test_lock_free_pointer_api(boost::true_type)
{
    test_pointer_api< Wrapper, T >();
}

template< template< typename > class Wrapper, typename T >
inline void test_lock_free_pointer_api(boost::false_type)
{
}

template< template< typename > class Wrapper, typename T >
inline void test_lock_free_pointer_api(void)
{
    test_lock_free_pointer_api< Wrapper, T >(boost::integral_constant< bool, Wrapper< T >::atomic_type::is_always_lock_free >());
}


template< template< typename > class Wrapper >
void test_enum_api(void)
{
    test_base_operators< Wrapper >(foo, bar, baz);
}

template< template< typename > class Wrapper >
inline void test_lock_free_enum_api(boost::true_type)
{
    test_enum_api< Wrapper >();
}

template< template< typename > class Wrapper >
inline void test_lock_free_enum_api(boost::false_type)
{
}

template< template< typename > class Wrapper >
inline void test_lock_free_enum_api(void)
{
    test_lock_free_enum_api< Wrapper >(boost::integral_constant< bool, Wrapper< test_enum >::atomic_type::is_always_lock_free >());
}


template< typename T >
struct test_struct
{
    typedef T value_type;
    value_type i;
    inline bool operator==(test_struct const& c) const { return i == c.i; }
    inline bool operator!=(test_struct const& c) const { return !operator==(c); }
};

template< typename Char, typename Traits, typename T >
inline std::basic_ostream< Char, Traits >& operator<< (std::basic_ostream< Char, Traits >& strm, test_struct< T > const& s)
{
    test_stream << "{" << s.i << "}";
    return strm;
}

template< template< typename > class Wrapper, typename T >
void test_struct_api(void)
{
    T a = {1}, b = {2}, c = {3};

    test_base_operators< Wrapper >(a, b, c);

    {
        Wrapper<T> wrapper_sa;
        typename Wrapper<T>::atomic_reference_type sa = wrapper_sa.a;
        Wrapper<typename T::value_type> wrapper_si;
        typename Wrapper<typename T::value_type>::atomic_reference_type si = wrapper_si.a;
        BOOST_TEST_EQ( sa.is_lock_free(), si.is_lock_free() );
    }
}

template< typename T >
struct test_struct_x2
{
    typedef T value_type;
    value_type i, j;
    inline bool operator==(test_struct_x2 const& c) const { return i == c.i && j == c.j; }
    inline bool operator!=(test_struct_x2 const& c) const { return !operator==(c); }
};

template< typename Char, typename Traits, typename T >
inline std::basic_ostream< Char, Traits >& operator<< (std::basic_ostream< Char, Traits >& strm, test_struct_x2< T > const& s)
{
    test_stream << "{" << s.i << ", " << s.j << "}";
    return strm;
}

template< template< typename > class Wrapper, typename T >
void test_struct_x2_api(void)
{
    T a = {1, 1}, b = {2, 2}, c = {3, 3};

    test_base_operators< Wrapper >(a, b, c);
}

struct large_struct
{
    unsigned char data[256u];

    inline bool operator==(large_struct const& c) const
    {
        return std::memcmp(data, &c.data, sizeof(data)) == 0;
    }
    inline bool operator!=(large_struct const& c) const
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

template< template< typename > class Wrapper >
void test_large_struct_api(void)
{
    large_struct a = {{1}}, b = {{2}}, c = {{3}};
    test_base_operators< Wrapper >(a, b, c);
}

struct test_struct_with_ctor
{
    typedef unsigned int value_type;
    value_type i;
    test_struct_with_ctor() : i(0x01234567) {}
    inline bool operator==(test_struct_with_ctor const& c) const { return i == c.i; }
    inline bool operator!=(test_struct_with_ctor const& c) const { return !operator==(c); }
};

template< typename Char, typename Traits >
inline std::basic_ostream< Char, Traits >& operator<< (std::basic_ostream< Char, Traits >& strm, test_struct_with_ctor const& s)
{
    strm << "{" << s.i << "}";
    return strm;
}

template< template< typename > class Wrapper >
void test_struct_with_ctor_api(void)
{
    {
        test_struct_with_ctor s;
        Wrapper<test_struct_with_ctor> wrapper_sa;
        typename Wrapper<test_struct_with_ctor>::atomic_reference_type sa = wrapper_sa.a;
        // Check that the default constructor was called
        BOOST_TEST( sa.load() == s );
    }

    test_struct_with_ctor a, b, c;
    a.i = 1;
    b.i = 2;
    c.i = 3;

    test_base_operators< Wrapper >(a, b, c);
}

#endif
