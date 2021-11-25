// Copyright 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/endian/conversion.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/cstdint.hpp>
#include <cstddef>
#include <ostream>
#include <iomanip>

class byte_span
{
private:

    unsigned char const * p_;
    std::size_t n_;

public:

    byte_span( unsigned char const * p, std::size_t n ): p_( p ), n_( n )
    {
    }

    template<std::size_t N> explicit byte_span( unsigned char const (&a)[ N ] ): p_( a ), n_( N )
    {
    }

    bool operator==( byte_span const& r ) const
    {
        if( n_ != r.n_ ) return false;

        for( std::size_t i = 0; i < n_; ++i )
        {
            if( p_[ i ] != r.p_[ i ] ) return false;
        }

        return true;
    }

    friend std::ostream& operator<<( std::ostream& os, byte_span s )
    {
        if( s.n_ == 0 ) return os;

        os << std::hex << std::setfill( '0' ) << std::uppercase;

        os << std::setw( 2 ) << +s.p_[ 0 ];

        for( std::size_t i = 1; i < s.n_; ++i )
        {
            os << ':' << std::setw( 2 ) << +s.p_[ i ];
        }

        os << std::dec << std::setfill( ' ' ) << std::nouppercase;;

        return os;
    }
};

int main()
{
    using namespace boost::endian;

    // 16

    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA };

        store_little_s16( v, -3343 );

        unsigned char w1[] = { 0xF1, 0xF2, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w1 ) );

        store_little_u16( v, 0x0201 );

        unsigned char w2[] = { 0x01, 0x02, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w2 ) );

        store_big_s16( v, -3343 );

        unsigned char w3[] = { 0xF2, 0xF1, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w3 ) );

        store_big_u16( v, 0x0201 );

        unsigned char w4[] = { 0x02, 0x01, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w4 ) );
    }

    // 24

    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA, 0xAA };

        store_little_s24( v, -789775 );

        unsigned char w1[] = { 0xF1, 0xF2, 0xF3, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w1 ) );

        store_little_u24( v, 0x030201 );

        unsigned char w2[] = { 0x01, 0x02, 0x03, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w2 ) );

        store_big_s24( v, -789775 );

        unsigned char w3[] = { 0xF3, 0xF2, 0xF1, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w3 ) );

        store_big_u24( v, 0x030201 );

        unsigned char w4[] = { 0x03, 0x02, 0x01, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w4 ) );
    }

    // 32

    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };

        store_little_s32( v, 0xF4F3F2F1 );

        unsigned char w1[] = { 0xF1, 0xF2, 0xF3, 0xF4, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w1 ) );

        store_little_u32( v, 0x04030201 );

        unsigned char w2[] = { 0x01, 0x02, 0x03, 0x04, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w2 ) );

        store_big_s32( v, 0xF4F3F2F1 );

        unsigned char w3[] = { 0xF4, 0xF3, 0xF2, 0xF1, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w3 ) );

        store_big_u32( v, 0x04030201 );

        unsigned char w4[] = { 0x04, 0x03, 0x02, 0x01, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w4 ) );
    }

    // 40

    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };

        store_little_s40( v, -43135012111 );

        unsigned char w1[] = { 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w1 ) );

        store_little_u40( v, 0x0504030201 );

        unsigned char w2[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w2 ) );

        store_big_s40( v, -43135012111 );

        unsigned char w3[] = { 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w3 ) );

        store_big_u40( v, 0x0504030201 );

        unsigned char w4[] = { 0x05, 0x04, 0x03, 0x02, 0x01, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w4 ) );
    }

    // 48

    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };

        store_little_s48( v, -9938739662095 );

        unsigned char w1[] = { 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w1 ) );

        store_little_u48( v, 0x060504030201 );

        unsigned char w2[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w2 ) );

        store_big_s48( v, -9938739662095 );

        unsigned char w3[] = { 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w3 ) );

        store_big_u48( v, 0x060504030201 );

        unsigned char w4[] = { 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w4 ) );
    }

    // 56

    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };

        store_little_s56( v, -2261738553347343 );

        unsigned char w1[] = { 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w1 ) );

        store_little_u56( v, 0x07060504030201 );

        unsigned char w2[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w2 ) );

        store_big_s56( v, -2261738553347343 );

        unsigned char w3[] = { 0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w3 ) );

        store_big_u56( v, 0x07060504030201 );

        unsigned char w4[] = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w4 ) );
    }

    // 64

    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };

        store_little_s64( v, 0xF8F7F6F5F4F3F2F1 );

        unsigned char w1[] = { 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w1 ) );

        store_little_u64( v, 0x0807060504030201 );

        unsigned char w2[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w2 ) );

        store_big_s64( v, 0xF8F7F6F5F4F3F2F1 );

        unsigned char w3[] = { 0xF8, 0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w3 ) );

        store_big_u64( v, 0x0807060504030201 );

        unsigned char w4[] = { 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w4 ) );
    }

    return boost::report_errors();
}
