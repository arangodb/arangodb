// Test for boost/core/bit.hpp (bit_floor)
//
// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/bit.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/detail/splitmix64.hpp>
#include <boost/cstdint.hpp>
#include <limits>

template<class T> void test_bit_floor( T x )
{
    T y = boost::core::bit_floor( x );

    if( x == 0 )
    {
        BOOST_TEST_EQ( y, 0 );
    }
    else
    {
        BOOST_TEST( boost::core::has_single_bit( y ) );
        BOOST_TEST_LE( +y, +x );
        BOOST_TEST_GT( +y, x >> 1 );
    }
}

int main()
{
    {
        test_bit_floor( static_cast<unsigned char>( 0 ) );
        test_bit_floor( static_cast<unsigned short>( 0 ) );
        test_bit_floor( static_cast<unsigned int>( 0 ) );
        test_bit_floor( static_cast<unsigned long>( 0 ) );
        test_bit_floor( static_cast<unsigned long long>( 0 ) );
    }

    boost::detail::splitmix64 rng;

    for( int i = 0; i < 1000; ++i )
    {
        boost::uint64_t x = rng();

        test_bit_floor( static_cast<unsigned char>( x ) );
        test_bit_floor( static_cast<unsigned short>( x ) );
        test_bit_floor( static_cast<unsigned int>( x ) );
        test_bit_floor( static_cast<unsigned long>( x ) );
        test_bit_floor( static_cast<unsigned long long>( x ) );
    }

    return boost::report_errors();
}
