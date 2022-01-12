// Test for boost/core/bit.hpp (countl_zero, countl_one)
//
// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/bit.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/detail/splitmix64.hpp>
#include <boost/cstdint.hpp>
#include <limits>

template<class T> void test_countl( T x )
{
    x |= static_cast<T>( 1 ) << ( std::numeric_limits<T>::digits - 1 );

    for( int i = 0; i <= std::numeric_limits<T>::digits; ++i, x >>= 1 )
    {
        BOOST_TEST_EQ( boost::core::countl_zero( x ), i );
        BOOST_TEST_EQ( boost::core::countl_one( static_cast<T>( ~x ) ), i );
    }
}

int main()
{
    test_countl( static_cast<unsigned char>( 0 ) );
    test_countl( static_cast<unsigned short>( 0 ) );
    test_countl( static_cast<unsigned int>( 0 ) );
    test_countl( static_cast<unsigned long>( 0 ) );
    test_countl( static_cast<unsigned long long>( 0 ) );

    boost::detail::splitmix64 rng;

    for( int i = 0; i < 1000; ++i )
    {
        boost::uint64_t x = rng();

        test_countl( static_cast<unsigned char>( x ) );
        test_countl( static_cast<unsigned short>( x ) );
        test_countl( static_cast<unsigned int>( x ) );
        test_countl( static_cast<unsigned long>( x ) );
        test_countl( static_cast<unsigned long long>( x ) );
    }

    return boost::report_errors();
}
