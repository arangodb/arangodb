
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

template<class V, class T, class A> constexpr T test( A&& a )
{
    V v;

    v = std::forward<A>(a);

    return get<T>(v);
}

int main()
{
    {
        constexpr auto w = test<variant<int>, int>( variant<int>( 1 ) );
        STATIC_ASSERT( w == 1 );
    }

    {
        constexpr auto w = test<variant<X>, X>( variant<X>( 1 ) );
        STATIC_ASSERT( w == 1 );
    }

#if defined( BOOST_LIBSTDCXX_VERSION ) && BOOST_LIBSTDCXX_VERSION < 50000
#else

    {
        constexpr auto w = test<variant<Y>, Y>( variant<Y>( 1 ) );
        STATIC_ASSERT( w == 1 );
    }

#endif

    {
        constexpr auto w = test<variant<int, float>, int>( variant<int, float>( 1 ) );
        STATIC_ASSERT( w == 1 );
    }

    {
        constexpr auto w = test<variant<int, float>, float>( variant<int, float>( 3.0f ) );
        STATIC_ASSERT( w == 3.0f );
    }

    {
        constexpr auto w = test<variant<int, int, float>, float>( variant<int, int, float>( 3.0f ) );
        STATIC_ASSERT( w == 3.0f );
    }

    {
        constexpr auto w = test<variant<E, E, X>, X>( variant<E, E, X>( 1 ) );
        STATIC_ASSERT( w == 1 );
    }

    {
        constexpr auto w = test<variant<int, int, float, float, X>, X>( variant<int, int, float, float, X>( X(1) ) );
        STATIC_ASSERT( w == 1 );
    }

#if defined( BOOST_LIBSTDCXX_VERSION ) && BOOST_LIBSTDCXX_VERSION < 50000
#else

    {
        constexpr auto w = test<variant<E, E, Y>, Y>( variant<E, E, Y>( 1 ) );
        STATIC_ASSERT( w == 1 );
    }

    {
        constexpr auto w = test<variant<int, int, float, float, Y>, Y>( variant<int, int, float, float, Y>( Y(1) ) );
        STATIC_ASSERT( w == 1 );
    }

#endif
}
