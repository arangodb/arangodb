
//  Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/detail/config.hpp>
#include <boost/core/lightweight_test.hpp>
#include <tuple>

using boost::mp11::mp_size_t;
using boost::mp11::mp_for_each;
using boost::mp11::mp_with_index;
using boost::mp11::mp_iota_c;

struct F
{
    std::size_t i_;

    explicit F( std::size_t i ): i_( i ) {}

    template<std::size_t I> bool operator()( mp_size_t<I> ) const
    {
        BOOST_TEST_EQ( I, i_ );
        return false;
    }
};

struct G
{
    void operator()( mp_size_t<0> ) const
    {
    }

    template<std::size_t N> void operator()( mp_size_t<N> ) const
    {
        for( std::size_t i = 0; i < N; ++i )
        {
            mp_with_index<N>( i, F(i) );
            mp_with_index<mp_size_t<N>>( i, F(i) );
        }
    }
};

int main()
{
#if BOOST_MP11_WORKAROUND( BOOST_MP11_MSVC, < 1900 )

    G()( mp_size_t<1>{} );
    G()( mp_size_t<2>{} );
    G()( mp_size_t<3>{} );
    G()( mp_size_t<4>{} );
    G()( mp_size_t<5>{} );
    G()( mp_size_t<6>{} );
    G()( mp_size_t<7>{} );
    G()( mp_size_t<8>{} );
    G()( mp_size_t<9>{} );
    G()( mp_size_t<10>{} );
    G()( mp_size_t<11>{} );
    G()( mp_size_t<12>{} );
    G()( mp_size_t<13>{} );
    G()( mp_size_t<14>{} );
    G()( mp_size_t<15>{} );
    G()( mp_size_t<16>{} );

    G()( mp_size_t<32+1>{} );

    G()( mp_size_t<48+2>{} );

    G()( mp_size_t<64+3>{} );

    G()( mp_size_t<96+4>{} );

    G()( mp_size_t<112+5>{} );

    G()( mp_size_t<128+6>{} );

#else

    mp_for_each<mp_iota_c<134>>( G() );

#endif

    return boost::report_errors();
}
