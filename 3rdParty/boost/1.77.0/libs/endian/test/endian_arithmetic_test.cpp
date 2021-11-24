// Copyright 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/endian/arithmetic.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/cstdint.hpp>
#include <cstddef>

template<BOOST_SCOPED_ENUM(boost::endian::order) Order, BOOST_SCOPED_ENUM(boost::endian::align) Align, class T> void test_arithmetic_( T const& x )
{
    boost::endian::endian_arithmetic<Order, T, sizeof(T) * 8, Align> y( x );

    BOOST_TEST_EQ( +x, +y );

    BOOST_TEST_EQ( x + x, y + y );
    BOOST_TEST_EQ( x - x, y - y );

    BOOST_TEST_EQ( x * x, y * y );
    BOOST_TEST_EQ( x / x, y / y );

    {
        T x2( x );
        boost::endian::endian_arithmetic<Order, T, sizeof(T) * 8, Align> y2( y );

        BOOST_TEST_EQ( x2 += x, y2 += y );
    }

    {
        T x2( x );
        boost::endian::endian_arithmetic<Order, T, sizeof(T) * 8, Align> y2( y );

        BOOST_TEST_EQ( x2 -= x, y2 -= y );
    }

    {
        T x2( x );
        boost::endian::endian_arithmetic<Order, T, sizeof(T) * 8, Align> y2( y );

        BOOST_TEST_EQ( x2 *= x, y2 *= y );
    }

    {
        T x2( x );
        boost::endian::endian_arithmetic<Order, T, sizeof(T) * 8, Align> y2( y );

        BOOST_TEST_EQ( x2 /= x, y2 /= y );
    }

    {
        T x2( x );
        boost::endian::endian_arithmetic<Order, T, sizeof(T) * 8, Align> y2( y );

        BOOST_TEST_EQ( ++x2, ++y2 );
    }

    {
        T x2( x );
        boost::endian::endian_arithmetic<Order, T, sizeof(T) * 8, Align> y2( y );

        BOOST_TEST_EQ( --x2, --y2 );
    }

    {
        T x2( x );
        boost::endian::endian_arithmetic<Order, T, sizeof(T) * 8, Align> y2( y );

        BOOST_TEST_EQ( x2++, y2++ );
    }

    {
        T x2( x );
        boost::endian::endian_arithmetic<Order, T, sizeof(T) * 8, Align> y2( y );

        BOOST_TEST_EQ( x2--, y2-- );
    }
}

template<BOOST_SCOPED_ENUM(boost::endian::order) Order, BOOST_SCOPED_ENUM(boost::endian::align) Align, class T> void test_integral_( T const& x )
{
    boost::endian::endian_arithmetic<Order, T, sizeof(T) * 8, Align> y( x );

    BOOST_TEST_EQ( x % x, y % y );

    BOOST_TEST_EQ( x & x, y & y );
    BOOST_TEST_EQ( x | x, y | y );
    BOOST_TEST_EQ( x ^ x, y ^ y );

    BOOST_TEST_EQ( x << 1, y << 1 );
    BOOST_TEST_EQ( x >> 1, y >> 1 );

    {
        T x2( x );
        boost::endian::endian_arithmetic<Order, T, sizeof(T) * 8, Align> y2( y );

        BOOST_TEST_EQ( x2 %= x, y2 %= y );
    }

    {
        T x2( x );
        boost::endian::endian_arithmetic<Order, T, sizeof(T) * 8, Align> y2( y );

        BOOST_TEST_EQ( x2 &= x, y2 &= y );
    }

    {
        T x2( x );
        boost::endian::endian_arithmetic<Order, T, sizeof(T) * 8, Align> y2( y );

        BOOST_TEST_EQ( x2 |= x, y2 |= y );
    }

    {
        T x2( x );
        boost::endian::endian_arithmetic<Order, T, sizeof(T) * 8, Align> y2( y );

        BOOST_TEST_EQ( x2 ^= x, y2 ^= y );
    }

    {
        T x2( x );
        boost::endian::endian_arithmetic<Order, T, sizeof(T) * 8, Align> y2( y );

        BOOST_TEST_EQ( x2 <<= 1, y2 <<= 1 );
    }

    {
        T x2( x );
        boost::endian::endian_arithmetic<Order, T, sizeof(T) * 8, Align> y2( y );

        BOOST_TEST_EQ( x2 >>= 1, y2 >>= 1 );
    }
}

template<class T> void test_arithmetic( T const& x )
{
    test_arithmetic_<boost::endian::order::little, boost::endian::align::no>( x );
    test_arithmetic_<boost::endian::order::little, boost::endian::align::yes>( x );
    test_arithmetic_<boost::endian::order::big, boost::endian::align::no>( x );
    test_arithmetic_<boost::endian::order::big, boost::endian::align::yes>( x );
}

template<class T> void test_integral( T const& x )
{
    test_arithmetic( x );

    test_integral_<boost::endian::order::little, boost::endian::align::no>( x );
    test_integral_<boost::endian::order::little, boost::endian::align::yes>( x );
    test_integral_<boost::endian::order::big, boost::endian::align::no>( x );
    test_integral_<boost::endian::order::big, boost::endian::align::yes>( x );
}

int main()
{
    test_integral( 0x7EF2 );
    test_integral( 0x01020304u );

    test_arithmetic( 3.1416f );
    test_arithmetic( 3.14159 );

    return boost::report_errors();
}
