// Copyright 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/endian/conversion.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/cstdint.hpp>
#include <cstddef>

template<class T> void test( T const& x )
{
    {
        unsigned char buffer[ sizeof(T) ];

        boost::endian::endian_store<T, sizeof(T), boost::endian::order::little>( buffer, x );
        T x2 = boost::endian::endian_load<T, sizeof(T), boost::endian::order::little>( buffer );

        BOOST_TEST_EQ( x, x2 );
    }

    {
        unsigned char buffer[ sizeof(T) ];

        boost::endian::endian_store<T, sizeof(T), boost::endian::order::big>( buffer, x );
        T x2 = boost::endian::endian_load<T, sizeof(T), boost::endian::order::big>( buffer );

        BOOST_TEST_EQ( x, x2 );
    }
}

enum E
{
    e = 0xF1F2F3
};

int main()
{
    test( 1.2e+34f );
    test( -1.234e+56 );
    test( e );

    return boost::report_errors();
}
