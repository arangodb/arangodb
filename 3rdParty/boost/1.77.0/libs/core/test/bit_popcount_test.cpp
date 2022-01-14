// Test for boost/core/bit.hpp (bit_ceil)
//
// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/bit.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/detail/splitmix64.hpp>
#include <boost/cstdint.hpp>
#include <limits>
#include <iostream>

template<class T> void test_popcount( T x )
{
    int k = 0;
    for( T y = x; y; y &= y - 1, ++k );

    BOOST_TEST_EQ( boost::core::popcount( x ), k ) || ( std::cerr << "x: " << +x << std::endl );
}

int main()
{
    {
        test_popcount( static_cast<unsigned char>( 0 ) );
        test_popcount( static_cast<unsigned short>( 0 ) );
        test_popcount( static_cast<unsigned int>( 0 ) );
        test_popcount( static_cast<unsigned long>( 0 ) );
        test_popcount( static_cast<unsigned long long>( 0 ) );
    }

    boost::detail::splitmix64 rng;

    for( int i = 0; i < 1000; ++i )
    {
        boost::uint64_t x = rng();

        test_popcount( static_cast<unsigned char>( x ) );
        test_popcount( static_cast<unsigned short>( x ) );
        test_popcount( static_cast<unsigned int>( x ) );
        test_popcount( static_cast<unsigned long>( x ) );
        test_popcount( static_cast<unsigned long long>( x ) );
    }

    return boost::report_errors();
}
