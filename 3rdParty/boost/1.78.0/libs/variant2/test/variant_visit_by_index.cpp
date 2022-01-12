// Copyright 2017, 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/mp11.hpp>
#include <boost/config.hpp>

using namespace boost::variant2;
using boost::mp11::mp_int;

struct X
{
};

struct F1
{
    int operator()( X& ) const { return 1; }
    int operator()( X const& ) const { return 2; }
    int operator()( X&& ) const { return 3; }
    int operator()( X const&& ) const { return 4; }
};

struct F2
{
    mp_int<1> operator()( X& ) const { return {}; }
    mp_int<2> operator()( X const& ) const { return {}; }
    mp_int<3> operator()( X&& ) const { return {}; }
    mp_int<4> operator()( X const&& ) const { return {}; }
};

int main()
{
    {
        variant<int, int, float> v;

        visit_by_index( v,
            []( int& x ){ BOOST_TEST_EQ( x, 0 ); },
            []( int&   ){ BOOST_ERROR( "incorrect alternative" ); },
            []( float& ){ BOOST_ERROR( "incorrect alternative" ); } );
    }

    {
        variant<int const, int, float const> v( in_place_index_t<0>(), 1 );

        visit_by_index( v,
            []( int const& x ){ BOOST_TEST_EQ( x, 1 ); },
            []( int&         ){ BOOST_ERROR( "incorrect alternative" ); },
            []( float const& ){ BOOST_ERROR( "incorrect alternative" ); } );
    }

    {
        variant<int, int, float> const v( in_place_index_t<1>(), 2 );

        visit_by_index( v,
            []( int const&   ){ BOOST_ERROR( "incorrect alternative" ); },
            []( int const& x ){ BOOST_TEST_EQ( x, 2 ); },
            []( float const& ){ BOOST_ERROR( "incorrect alternative" ); } );
    }

    {
        variant<int const, int, float const> const v( 3.14f );

        visit_by_index( v,
            []( int const&     ){ BOOST_ERROR( "incorrect alternative" ); },
            []( int const&     ){ BOOST_ERROR( "incorrect alternative" ); },
            []( float const& x ){ BOOST_TEST_EQ( x, 3.14f ); } );
    }

    {
        variant<int, float> const v( 7 );

        auto r = visit_by_index( v,
            []( int const& x   ) -> double { return x; },
            []( float const& x ) -> double { return x; } );

        BOOST_TEST_TRAIT_SAME( decltype(r), double );
        BOOST_TEST_EQ( r, 7.0 );
    }

    {
        variant<int, float> const v( 2.0f );

        auto r = visit_by_index( v,
            []( int const& x   ) { return x + 0.0; },
            []( float const& x ) { return x + 0.0; } );

        BOOST_TEST_TRAIT_SAME( decltype(r), double );
        BOOST_TEST_EQ( r, 2.0 );
    }

    {
        variant<int, float, double> const v( 3.0 );

        auto r = visit_by_index<double>( v,
            []( int const& x    ) { return x; },
            []( float const& x  ) { return x; },
            []( double const& x ) { return x; } );

        BOOST_TEST_TRAIT_SAME( decltype(r), double );
        BOOST_TEST_EQ( r, 3.0 );
    }

    {
        variant<X> v;
        variant<X> const cv;

        F1 f1;

        BOOST_TEST_EQ( visit_by_index( v, f1 ), 1 );
        BOOST_TEST_EQ( visit_by_index( cv, f1 ), 2 );
        BOOST_TEST_EQ( visit_by_index( std::move( v ), f1 ), 3 );
        BOOST_TEST_EQ( visit_by_index( std::move( cv ), f1 ), 4 );

        F2 f2;

        BOOST_TEST_EQ( visit_by_index<int>( v, f2 ), 1 );
        BOOST_TEST_EQ( visit_by_index<int>( cv, f2 ), 2 );
        BOOST_TEST_EQ( visit_by_index<int>( std::move( v ), f2 ), 3 );
        BOOST_TEST_EQ( visit_by_index<int>( std::move( cv ), f2 ), 4 );
    }

    return boost::report_errors();
}
