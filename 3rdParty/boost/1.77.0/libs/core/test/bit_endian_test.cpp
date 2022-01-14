// Test for boost/core/bit.hpp (bit_ceil)
//
// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/bit.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/cstdint.hpp>
#include <cstring>

int main()
{
    boost::uint64_t v = static_cast<boost::uint64_t>( 0x0102030405060708ull );

    if( boost::core::endian::native == boost::core::endian::little )
    {
        unsigned char w[] = { 8, 7, 6, 5, 4, 3, 2, 1 };
        BOOST_TEST( std::memcmp( &v, w, 8 ) == 0 );
    }
    else if( boost::core::endian::native == boost::core::endian::big )
    {
        unsigned char w[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
        BOOST_TEST( std::memcmp( &v, w, 8 ) == 0 );
    }
    else
    {
        unsigned char w1[] = { 8, 7, 6, 5, 4, 3, 2, 1 };
        BOOST_TEST( std::memcmp( &v, w1, 8 ) != 0 );

        unsigned char w2[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
        BOOST_TEST( std::memcmp( &v, w2, 8 ) != 0 );
    }

    return boost::report_errors();
}
