// Copyright 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/endian/detail/order.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/cstdint.hpp>
#include <cstddef>
#include <ostream>
#include <iomanip>

#if defined(_MSC_VER)
# pragma warning(disable: 4127)  // conditional expression is constant
#endif

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

        os << std::dec << std::setfill( ' ' ) << std::nouppercase;

        return os;
    }
};

int main()
{
    boost::uint64_t v = static_cast<boost::uint64_t>( 0x0102030405060708ull );
    byte_span v2( reinterpret_cast<unsigned char const*>( &v ), sizeof(v) );

    if( boost::endian::order::native == boost::endian::order::little )
    {
        unsigned char w[] = { 8, 7, 6, 5, 4, 3, 2, 1 };
        BOOST_TEST_EQ( v2, byte_span( w ) );
    }
    else if( boost::endian::order::native == boost::endian::order::big )
    {
        unsigned char w[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
        BOOST_TEST_EQ( v2, byte_span( w ) );
    }
    else
    {
        BOOST_ERROR( "boost::endian::order::native is neither big nor little" );
    }

    return boost::report_errors();
}
