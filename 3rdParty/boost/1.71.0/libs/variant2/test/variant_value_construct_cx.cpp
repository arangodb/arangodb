
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
    constexpr operator int() const { return 2; }
};

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

int main()
{
    {
        constexpr variant<int> v( 1 );

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( get<0>(v) == 1 );
    }

    {
        constexpr variant<int> v( 'a' );

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( get<0>(v) == 'a' );
    }

    {
        constexpr variant<int> v( X{} );

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( get<0>(v) == 2 );
    }

    {
        constexpr variant<int const> v( 1 );

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( get<0>(v) == 1 );
    }

    {
        constexpr variant<int const> v( 'a' );

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( get<0>(v) == 'a' );
    }

    {
        constexpr variant<int const> v( X{} );

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( get<0>(v) == 2 );
    }

    {
        constexpr variant<int, float, X> v( 1 );

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( holds_alternative<int>(v) );
        STATIC_ASSERT( get<0>(v) == 1 );
    }

    {
        constexpr variant<int, float, X> v( 'a' );

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( holds_alternative<int>(v) );
        STATIC_ASSERT( get<0>(v) == 'a' );
    }

    {
        constexpr variant<int, float, X> v( 3.14f );

        STATIC_ASSERT( v.index() == 1 );
        STATIC_ASSERT( holds_alternative<float>(v) );
        STATIC_ASSERT( get<1>(v) == 3.14f );
    }

    {
        constexpr variant<int, float, X> v( X{} );

        STATIC_ASSERT( v.index() == 2 );
        STATIC_ASSERT( holds_alternative<X>(v) );
    }

    {
        constexpr variant<int, int, float, X> v( 3.14f );

        STATIC_ASSERT( v.index() == 2 );
        STATIC_ASSERT( holds_alternative<float>(v) );
        STATIC_ASSERT( get<2>(v) == 3.14f );
    }

    {
        constexpr variant<int, int, float, X> v( X{} );

        STATIC_ASSERT( v.index() == 3 );
        STATIC_ASSERT( holds_alternative<X>(v) );
    }
}
