
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>

using namespace boost::variant2;

struct X
{
    int v;
    X() = default;
    constexpr X( int v ): v( v ) {}
    constexpr operator int() const { return v; }
};

struct Y
{
    int v;
    constexpr Y(): v() {}
    constexpr Y( int v ): v( v ) {}
    constexpr operator int() const { return v; }
};

enum E
{
    v
};

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

template<class T, class V> constexpr T test( V const v )
{
    return get<T>( v );
}

int main()
{
    {
        constexpr variant<int> v( 1 );
        constexpr auto w = test<int>( v );
        STATIC_ASSERT( w == 1 );
    }

    {
        constexpr variant<X> v( 1 );
        constexpr auto w = test<X>( v );
        STATIC_ASSERT( w == 1 );
    }

#if defined( BOOST_LIBSTDCXX_VERSION ) && BOOST_LIBSTDCXX_VERSION < 50000
#else

    {
        constexpr variant<Y> v( 1 );
        constexpr auto w = test<Y>( v );
        STATIC_ASSERT( w == 1 );
    }

#endif

    {
        constexpr variant<int, float> v( 1 );
        constexpr auto w = test<int>( v );
        STATIC_ASSERT( w == 1 );
    }

    {
        constexpr variant<int, float> v( 3.0f );
        constexpr auto w = test<float>( v );
        STATIC_ASSERT( w == 3.0f );
    }

    {
        constexpr variant<int, int, float> v( 3.0f );
        constexpr auto w = test<float>( v );
        STATIC_ASSERT( w == 3.0f );
    }

    {
        constexpr variant<E, E, X> v( 1 );
        constexpr auto w = test<X>( v );
        STATIC_ASSERT( w == 1 );
    }

    {
        constexpr variant<int, int, float, float, X> v( X(1) );
        constexpr auto w = test<X>( v );
        STATIC_ASSERT( w == 1 );
    }

#if defined( BOOST_LIBSTDCXX_VERSION ) && BOOST_LIBSTDCXX_VERSION < 50000
#else

    {
        constexpr variant<E, E, Y> v( 1 );
        constexpr auto w = test<Y>( v );
        STATIC_ASSERT( w == 1 );
    }

    {
        constexpr variant<int, int, float, float, Y> v( Y(1) );
        constexpr auto w = test<Y>( v );
        STATIC_ASSERT( w == 1 );
    }

#endif
}
