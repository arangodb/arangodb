// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/tuple.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/core/lightweight_test.hpp>
#include <tuple>
#include <utility>
#include <array>
#include <cstddef>

int f( int x )
{
    return x + 1;
}

int g( int x, int y )
{
    return x + y;
}

int h( int x, int y, int z )
{
    return x + y + z;
}

int q( int x, int y, int z, int v )
{
    return x + y + z + v;
}

template<class T1, class... T> std::array<T1, 1 + sizeof...(T)> make_array( T1 t1, T... t )
{
    return { { t1, t... } };
}

template<class Tp> struct test_element
{
    Tp& tp;

    template<int I> void operator()( boost::mp11::mp_int<I> ) const
    {
        BOOST_TEST_EQ( std::get<I>( tp ), q( I, I, I, I ) );
    }
};

int main()
{
    using boost::mp11::tuple_transform;

    //

    {
        std::tuple<int> r = tuple_transform( f, std::make_tuple( 1 ) );

        BOOST_TEST_EQ( std::get<0>( r ), 2 );
    }

    {
        std::tuple<int> r = tuple_transform( f, ::make_array( 1 ) );

        BOOST_TEST_EQ( std::get<0>( r ), 2 );
    }

    {
        std::tuple<int> r = tuple_transform( g, ::make_array( 1 ), std::make_tuple( 2 ) );

        BOOST_TEST_EQ( std::get<0>( r ), 3 );
    }

    {
        std::tuple<int> r = tuple_transform( h, ::make_array( 1 ), std::make_tuple( 2 ), ::make_array( 3 ) );

        BOOST_TEST_EQ( std::get<0>( r ), 6 );
    }

    //

    {
        std::tuple<int, int> r = tuple_transform( f, std::make_tuple( 1, 2 ) );

        BOOST_TEST_EQ( std::get<0>( r ), 2 );
        BOOST_TEST_EQ( std::get<1>( r ), 3 );
    }

    {
        std::tuple<int, int> r = tuple_transform( f, std::make_pair( 1, 2 ) );

        BOOST_TEST_EQ( std::get<0>( r ), 2 );
        BOOST_TEST_EQ( std::get<1>( r ), 3 );
    }

    {
        std::tuple<int, int> r = tuple_transform( f, ::make_array( 1, 2 ) );

        BOOST_TEST_EQ( std::get<0>( r ), 2 );
        BOOST_TEST_EQ( std::get<1>( r ), 3 );
    }

    {
        std::tuple<int, int> r = tuple_transform( g, ::make_array( 1, 2 ), std::make_pair( 3, 4 ) );

        BOOST_TEST_EQ( std::get<0>( r ), 4 );
        BOOST_TEST_EQ( std::get<1>( r ), 6 );
    }

    {
        std::tuple<int, int> r = tuple_transform( h, ::make_array( 1, 2 ), std::make_pair( 3, 4 ), std::make_tuple( 5, 6 ) );

        BOOST_TEST_EQ( std::get<0>( r ), 9 );
        BOOST_TEST_EQ( std::get<1>( r ), 12 );
    }

    //

    {
        std::tuple<int, int, int> r = tuple_transform( f, std::make_tuple( 1, 2, 3 ) );

        BOOST_TEST_EQ( std::get<0>( r ), 2 );
        BOOST_TEST_EQ( std::get<1>( r ), 3 );
        BOOST_TEST_EQ( std::get<2>( r ), 4 );
    }

    {
        std::tuple<int, int, int> r = tuple_transform( f, ::make_array( 1, 2, 3 ) );

        BOOST_TEST_EQ( std::get<0>( r ), 2 );
        BOOST_TEST_EQ( std::get<1>( r ), 3 );
        BOOST_TEST_EQ( std::get<2>( r ), 4 );
    }

    {
        std::tuple<int, int, int> r = tuple_transform( g, ::make_array( 1, 2, 3 ), std::make_tuple( 4, 5, 6 ) );

        BOOST_TEST_EQ( std::get<0>( r ), 5 );
        BOOST_TEST_EQ( std::get<1>( r ), 7 );
        BOOST_TEST_EQ( std::get<2>( r ), 9 );
    }

    {
        std::tuple<int, int, int> r = tuple_transform( h, ::make_array( 1, 2, 3 ), std::make_tuple( 4, 5, 6 ), ::make_array( 7, 8, 9 ) );

        BOOST_TEST_EQ( std::get<0>( r ), 12 );
        BOOST_TEST_EQ( std::get<1>( r ), 15 );
        BOOST_TEST_EQ( std::get<2>( r ), 18 );
    }

#if !BOOST_MP11_WORKAROUND( BOOST_MP11_MSVC, < 1900 )

    {
        using namespace boost::mp11;

        int const N = 12;

        using Tp = mp_rename<mp_iota< mp_int<N> >, std::tuple>;

        auto const r = tuple_transform( q, Tp(), Tp(), Tp(), Tp() );

        mp_for_each<Tp>( test_element<decltype(r)>{ r } );
    }

#endif

    return boost::report_errors();
}
