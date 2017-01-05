//  Copyright (c) 2011 Helge Bahmann
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_ATOMIC_API_TEST_HELPERS_HPP
#define BOOST_ATOMIC_API_TEST_HELPERS_HPP

#include <boost/atomic.hpp>
#include <cstring>
#include <boost/config.hpp>
#include <boost/cstdint.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/type_traits/integral_constant.hpp>
#include <boost/type_traits/is_unsigned.hpp>

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
        BOOST_TEST( a.load() == value1 );
    }

    {
        boost::atomic<T> a(value1);
        a.store(value2);
        BOOST_TEST( a.load() == value2 );
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
        BOOST_TEST( a.load() == value2 && n == value1 );
    }

    {
        boost::atomic<T> a(value1);
        T expected = value1;
        bool success = a.compare_exchange_strong(expected, value3);
        BOOST_TEST( success );
        BOOST_TEST( a.load() == value3 && expected == value1 );
    }

    {
        boost::atomic<T> a(value1);
        T expected = value2;
        bool success = a.compare_exchange_strong(expected, value3);
        BOOST_TEST( !success );
        BOOST_TEST( a.load() == value1 && expected == value1 );
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
        BOOST_TEST( a.load() == value3 && expected == value1 );
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
        BOOST_TEST( a.load() == value1 && expected == value1 );
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
        BOOST_TEST( a.load() == T((AddType)value + delta) );
        BOOST_TEST( n == value );
    }

    {
        boost::atomic<T> a(value);
        T n = a.fetch_sub(delta);
        BOOST_TEST( a.load() == T((AddType)value - delta) );
        BOOST_TEST( n == value );
    }

    /* overloaded modify/assign*/
    {
        boost::atomic<T> a(value);
        T n = (a += delta);
        BOOST_TEST( a.load() == T((AddType)value + delta) );
        BOOST_TEST( n == T((AddType)value + delta) );
    }

    {
        boost::atomic<T> a(value);
        T n = (a -= delta);
        BOOST_TEST( a.load() == T((AddType)value - delta) );
        BOOST_TEST( n == T((AddType)value - delta) );
    }

    /* overloaded increment/decrement */
    {
        boost::atomic<T> a(value);
        T n = a++;
        BOOST_TEST( a.load() == T((AddType)value + 1) );
        BOOST_TEST( n == value );
    }

    {
        boost::atomic<T> a(value);
        T n = ++a;
        BOOST_TEST( a.load() == T((AddType)value + 1) );
        BOOST_TEST( n == T((AddType)value + 1) );
    }

    {
        boost::atomic<T> a(value);
        T n = a--;
        BOOST_TEST( a.load() == T((AddType)value - 1) );
        BOOST_TEST( n == value );
    }

    {
        boost::atomic<T> a(value);
        T n = --a;
        BOOST_TEST( a.load() == T((AddType)value - 1) );
        BOOST_TEST( n == T((AddType)value - 1) );
    }
}

template<typename T, typename D>
void test_additive_operators(T value, D delta)
{
    test_additive_operators_with_type<T, D, T>(value, delta);
}

template<typename T>
void test_additive_wrap(T value)
{
    {
        boost::atomic<T> a(value);
        T n = a.fetch_add(1) + 1;
        BOOST_TEST( a.compare_exchange_strong(n, n) );
    }
    {
        boost::atomic<T> a(value);
        T n = a.fetch_sub(1) - 1;
        BOOST_TEST( a.compare_exchange_strong(n, n) );
    }
}

template<typename T>
void test_bit_operators(T value, T delta)
{
    /* explicit and/or/xor */
    {
        boost::atomic<T> a(value);
        T n = a.fetch_and(delta);
        BOOST_TEST( a.load() == T(value & delta) );
        BOOST_TEST( n == value );
    }

    {
        boost::atomic<T> a(value);
        T n = a.fetch_or(delta);
        BOOST_TEST( a.load() == T(value | delta) );
        BOOST_TEST( n == value );
    }

    {
        boost::atomic<T> a(value);
        T n = a.fetch_xor(delta);
        BOOST_TEST( a.load() == T(value ^ delta) );
        BOOST_TEST( n == value );
    }

    /* overloaded modify/assign */
    {
        boost::atomic<T> a(value);
        T n = (a &= delta);
        BOOST_TEST( a.load() == T(value & delta) );
        BOOST_TEST( n == T(value & delta) );
    }

    {
        boost::atomic<T> a(value);
        T n = (a |= delta);
        BOOST_TEST( a.load() == T(value | delta) );
        BOOST_TEST( n == T(value | delta) );
    }

    {
        boost::atomic<T> a(value);
        T n = (a ^= delta);
        BOOST_TEST( a.load() == T(value ^ delta) );
        BOOST_TEST( n == T(value ^ delta) );
    }
}

template<typename T>
void do_test_integral_api(boost::false_type)
{
    BOOST_TEST( sizeof(boost::atomic<T>) >= sizeof(T));

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
    test_additive_wrap<T>(~(T)0u);
    test_additive_wrap<T>((~(T)0u) << (sizeof(T) * 8 - 1));
    test_additive_wrap<T>(~((~(T)0u) << (sizeof(T) * 8 - 1)));
}

template<typename T>
inline void test_integral_api(void)
{
    do_test_integral_api<T>(boost::is_unsigned<T>());
}

template<typename T>
void test_pointer_api(void)
{
    BOOST_TEST( sizeof(boost::atomic<T *>) >= sizeof(T *));
    BOOST_TEST( sizeof(boost::atomic<void *>) >= sizeof(T *));

    T values[3];

    test_base_operators<T*>(&values[0], &values[1], &values[2]);
    test_additive_operators<T*>(&values[1], 1);

    test_base_operators<void*>(&values[0], &values[1], &values[2]);
    test_additive_operators_with_type<void*, int, char*>(&values[1], 1);

#if defined(BOOST_HAS_INTPTR_T)
    boost::atomic<void *> ptr;
    boost::atomic<boost::intptr_t> integral;
    BOOST_TEST( ptr.is_lock_free() == integral.is_lock_free() );
#endif
}

enum test_enum {
    foo, bar, baz
};

static void
test_enum_api(void)
{
    test_base_operators(foo, bar, baz);
}

template<typename T>
struct test_struct {
    typedef T value_type;
    value_type i;
    inline bool operator==(const test_struct & c) const {return i == c.i;}
    inline bool operator!=(const test_struct & c) const {return i != c.i;}
};

template<typename T>
void
test_struct_api(void)
{
    T a = {1}, b = {2}, c = {3};

    test_base_operators(a, b, c);

    {
        boost::atomic<T> sa;
        boost::atomic<typename T::value_type> si;
        BOOST_TEST( sa.is_lock_free() == si.is_lock_free() );
    }
}

template<typename T>
struct test_struct_x2 {
    typedef T value_type;
    value_type i, j;
    inline bool operator==(const test_struct_x2 & c) const {return i == c.i && j == c.j;}
    inline bool operator!=(const test_struct_x2 & c) const {return i != c.i && j != c.j;}
};

template<typename T>
void
test_struct_x2_api(void)
{
    T a = {1, 1}, b = {2, 2}, c = {3, 3};

    test_base_operators(a, b, c);
}

struct large_struct {
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

static void
test_large_struct_api(void)
{
    large_struct a = {{1}}, b = {{2}}, c = {{3}};
    test_base_operators(a, b, c);
}

struct test_struct_with_ctor {
    typedef unsigned int value_type;
    value_type i;
    test_struct_with_ctor() : i(0x01234567) {}
    inline bool operator==(const test_struct_with_ctor & c) const {return i == c.i;}
    inline bool operator!=(const test_struct_with_ctor & c) const {return i != c.i;}
};

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
