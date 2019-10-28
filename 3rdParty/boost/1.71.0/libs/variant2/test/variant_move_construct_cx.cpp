
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>
#include <utility>

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

template<class T, class V> constexpr T test( V&& v )
{
    V v2( std::forward<V>(v) );
    return get<T>( v2 );
}

int main()
{
    {
        constexpr auto w = test<int>( variant<int>( 1 ) );
        STATIC_ASSERT( w == 1 );
    }

    {
        constexpr auto w = test<X>( variant<X>( 1 ) );
        STATIC_ASSERT( w == 1 );
    }

#if defined( BOOST_LIBSTDCXX_VERSION ) && BOOST_LIBSTDCXX_VERSION < 50000
#else

    {
        constexpr auto w = test<Y>( variant<Y>( 1 ) );
        STATIC_ASSERT( w == 1 );
    }

#endif

    {
        constexpr auto w = test<int>( variant<int, float>( 1 ) );
        STATIC_ASSERT( w == 1 );
    }

    {
        constexpr auto w = test<float>( variant<int, float>( 3.0f ) );
        STATIC_ASSERT( w == 3.0f );
    }

    {
        constexpr auto w = test<float>( variant<int, int, float>( 3.0f ) );
        STATIC_ASSERT( w == 3.0f );
    }

    {
        constexpr auto w = test<X>( variant<E, E, X>( 1 ) );
        STATIC_ASSERT( w == 1 );
    }

    {
        constexpr auto w = test<X>( variant<int, int, float, float, X>( X(1) ) );
        STATIC_ASSERT( w == 1 );
    }

#if defined( BOOST_LIBSTDCXX_VERSION ) && BOOST_LIBSTDCXX_VERSION < 50000
#else

    {
        constexpr auto w = test<Y>( variant<E, E, Y>( 1 ) );
        STATIC_ASSERT( w == 1 );
    }

    {
        constexpr auto w = test<Y>( variant<int, int, float, float, Y>( Y(1) ) );
        STATIC_ASSERT( w == 1 );
    }

#endif
}
