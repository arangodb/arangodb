// Test for boost/core/bit.hpp (bit_width)
//
// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/bit.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/detail/splitmix64.hpp>
#include <boost/cstdint.hpp>
#include <limits>

template<class T> void test_width( T x )
{
    BOOST_TEST_EQ( boost::core::bit_width( x ), std::numeric_limits<T>::digits - boost::core::countl_zero( x ) );
}

int main()
{
    {
        boost::uint8_t x = 0;

        BOOST_TEST_EQ( boost::core::bit_width( x ), 0 );

        x = 1;

        for( int i = 0; i < 8; ++i, x <<= 1 )
        {
            BOOST_TEST_EQ( boost::core::bit_width( x ), i+1 );
            BOOST_TEST_EQ( boost::core::bit_width( static_cast<boost::uint8_t>( x | ( x >> 1 ) ) ), i+1 );
            BOOST_TEST_EQ( boost::core::bit_width( static_cast<boost::uint8_t>( x | ( x >> 2 ) ) ), i+1 );
        }
    }

    {
        boost::uint16_t x = 0;

        BOOST_TEST_EQ( boost::core::bit_width( x ), 0 );

        x = 1;

        for( int i = 0; i < 16; ++i, x <<= 1 )
        {
            BOOST_TEST_EQ( boost::core::bit_width( x ), i+1 );
            BOOST_TEST_EQ( boost::core::bit_width( static_cast<boost::uint16_t>( x | ( x >> 1 ) ) ), i+1 );
            BOOST_TEST_EQ( boost::core::bit_width( static_cast<boost::uint16_t>( x | ( x >> 2 ) ) ), i+1 );
        }
    }

    {
        boost::uint32_t x = 0;

        BOOST_TEST_EQ( boost::core::bit_width( x ), 0 );

        x = 1;

        for( int i = 0; i < 32; ++i, x <<= 1 )
        {
            BOOST_TEST_EQ( boost::core::bit_width( x ), i+1 );
            BOOST_TEST_EQ( boost::core::bit_width( static_cast<boost::uint32_t>( x | ( x >> 1 ) ) ), i+1 );
            BOOST_TEST_EQ( boost::core::bit_width( static_cast<boost::uint32_t>( x | ( x >> 2 ) ) ), i+1 );
        }
    }

    {
        boost::uint64_t x = 0;

        BOOST_TEST_EQ( boost::core::bit_width( x ), 0 );

        x = 1;

        for( int i = 0; i < 64; ++i, x <<= 1 )
        {
            BOOST_TEST_EQ( boost::core::bit_width( x ), i+1 );
            BOOST_TEST_EQ( boost::core::bit_width( static_cast<boost::uint64_t>( x | ( x >> 1 ) ) ), i+1 );
            BOOST_TEST_EQ( boost::core::bit_width( static_cast<boost::uint64_t>( x | ( x >> 2 ) ) ), i+1 );
        }
    }

    boost::detail::splitmix64 rng;

    for( int i = 0; i < 1000; ++i )
    {
        boost::uint64_t x = rng();

        test_width( static_cast<unsigned char>( x ) );
        test_width( static_cast<unsigned short>( x ) );
        test_width( static_cast<unsigned int>( x ) );
        test_width( static_cast<unsigned long>( x ) );
        test_width( static_cast<unsigned long long>( x ) );
    }

    return boost::report_errors();
}
