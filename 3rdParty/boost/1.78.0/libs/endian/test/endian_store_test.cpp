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

template<class T> void test_1()
{
    {
        unsigned char v[] = { 0xAA, 0xAA };

        boost::endian::endian_store<T, 1, boost::endian::order::little>( v, 0x01 );

        unsigned char w[] = { 0x01, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w ) );
    }

    {
        unsigned char v[] = { 0xAA, 0xAA };

        boost::endian::endian_store<T, 1, boost::endian::order::big>( v, 0x01 );

        unsigned char w[] = { 0x01, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w ) );
    }
}

template<class T> void test_2()
{
    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA };

        boost::endian::endian_store<T, 2, boost::endian::order::little>( v, 0x0102 );

        unsigned char w[] = { 0x02, 0x01, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w ) );
    }

    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA };

        boost::endian::endian_store<T, 2, boost::endian::order::big>( v, 0x0102 );

        unsigned char w[] = { 0x01, 0x02, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w ) );
    }
}

template<class T> void test_3()
{
    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA, 0xAA };

        boost::endian::endian_store<T, 3, boost::endian::order::little>( v, 0x010203 );

        unsigned char w[] = { 0x03, 0x02, 0x01, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w ) );
    }

    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA, 0xAA };

        boost::endian::endian_store<T, 3, boost::endian::order::big>( v, 0x010203 );

        unsigned char w[] = { 0x01, 0x02, 0x03, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w ) );
    }
}

template<class T> void test_4()
{
    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };

        boost::endian::endian_store<T, 4, boost::endian::order::little>( v, 0x01020304 );

        unsigned char w[] = { 0x04, 0x03, 0x02, 0x01, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w ) );
    }

    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };

        boost::endian::endian_store<T, 4, boost::endian::order::big>( v, 0x01020304 );

        unsigned char w[] = { 0x01, 0x02, 0x03, 0x04, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w ) );
    }
}

template<class T> void test_5()
{
    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };

        boost::endian::endian_store<T, 5, boost::endian::order::little>( v, 0x0102030405 );

        unsigned char w[] = { 0x05, 0x04, 0x03, 0x02, 0x01, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w ) );
    }

    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };

        boost::endian::endian_store<T, 5, boost::endian::order::big>( v, 0x0102030405 );

        unsigned char w[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w ) );
    }
}

template<class T> void test_6()
{
    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };

        boost::endian::endian_store<T, 6, boost::endian::order::little>( v, 0x010203040506 );

        unsigned char w[] = { 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w ) );
    }

    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };

        boost::endian::endian_store<T, 6, boost::endian::order::big>( v, 0x010203040506 );

        unsigned char w[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w ) );
    }
}

template<class T> void test_7()
{
    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };

        boost::endian::endian_store<T, 7, boost::endian::order::little>( v, 0x01020304050607 );

        unsigned char w[] = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w ) );
    }

    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };

        boost::endian::endian_store<T, 7, boost::endian::order::big>( v, 0x01020304050607 );

        unsigned char w[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w ) );
    }
}

template<class T> void test_8()
{
    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };

        boost::endian::endian_store<T, 8, boost::endian::order::little>( v, 0x0102030405060708 );

        unsigned char w[] = { 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w ) );
    }

    {
        unsigned char v[] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };

        boost::endian::endian_store<T, 8, boost::endian::order::big>( v, 0x0102030405060708 );

        unsigned char w[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0xAA };

        BOOST_TEST_EQ( byte_span( v ), byte_span( w ) );
    }
}

int main()
{
    // 1

    test_1<boost::int8_t>();
    test_1<boost::uint8_t>();

    test_1<boost::int16_t>();
    test_1<boost::uint16_t>();

    test_1<boost::int32_t>();
    test_1<boost::uint32_t>();

    test_1<boost::int64_t>();
    test_1<boost::uint64_t>();

    // 2

    test_2<boost::int16_t>();
    test_2<boost::uint16_t>();

    test_2<boost::int32_t>();
    test_2<boost::uint32_t>();

    test_2<boost::int64_t>();
    test_2<boost::uint64_t>();

    // 3

    test_3<boost::int32_t>();
    test_3<boost::uint32_t>();

    test_3<boost::int64_t>();
    test_3<boost::uint64_t>();

    // 4

    test_4<boost::int32_t>();
    test_4<boost::uint32_t>();

    test_4<boost::int64_t>();
    test_4<boost::uint64_t>();

    // 5

    test_5<boost::int64_t>();
    test_5<boost::uint64_t>();

    // 6

    test_6<boost::int64_t>();
    test_6<boost::uint64_t>();

    // 7

    test_7<boost::int64_t>();
    test_7<boost::uint64_t>();

    // 8

    test_8<boost::int64_t>();
    test_8<boost::uint64_t>();

    return boost::report_errors();
}
