
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>

using namespace boost::variant2;

struct X
{
    constexpr X() = default;
    constexpr explicit X(int, int) {}
    X( in_place_index_t<0> ) = delete;
};

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

int main()
{
    {
        constexpr variant<int> v( in_place_index_t<0>{} );

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( get<0>(v) == 0 );
    }

    {
        constexpr variant<X> v( in_place_index_t<0>{} );

        STATIC_ASSERT( v.index() == 0 );
    }

    {
        constexpr variant<int> v( in_place_index_t<0>{}, 1 );

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( get<0>(v) == 1 );
    }

    {
        constexpr variant<int, float> v( in_place_index_t<0>{} );

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( get<0>(v) == 0 );
    }

    {
        constexpr variant<int, float> v( in_place_index_t<0>{}, 1 );

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( get<0>(v) == 1 );
    }

    {
        constexpr variant<int, float> v( in_place_index_t<1>{} );

        STATIC_ASSERT( v.index() == 1 );
        STATIC_ASSERT( get<1>(v) == 0 );
    }

    {
        constexpr variant<int, float> v( in_place_index_t<1>{}, 3.14f );

        STATIC_ASSERT( v.index() == 1 );
        STATIC_ASSERT( get<1>(v) == 3.14f );
    }

    {
        constexpr variant<int, int, float, float, X, X> v( in_place_index_t<0>{}, 1 );

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( get<0>(v) == 1 );
    }

    {
        constexpr variant<int, int, float, float, X, X> v( in_place_index_t<1>{}, 1 );

        STATIC_ASSERT( v.index() == 1 );
        STATIC_ASSERT( get<1>(v) == 1 );
    }

    {
        constexpr variant<int, int, float, float, X, X> v( in_place_index_t<2>{}, 3.14f );

        STATIC_ASSERT( v.index() == 2 );
        STATIC_ASSERT( get<2>(v) == 3.14f );
    }

    {
        constexpr variant<int, int, float, float, X, X> v( in_place_index_t<3>{}, 3.14f );

        STATIC_ASSERT( v.index() == 3 );
        STATIC_ASSERT( get<3>(v) == 3.14f );
    }

    {
        constexpr variant<int, int, float, float, X, X> v( in_place_index_t<4>{} );

        STATIC_ASSERT( v.index() == 4 );
    }

#if BOOST_WORKAROUND(BOOST_GCC, >= 100000 && BOOST_GCC < 120000)

    // no idea why this fails on g++ 10/11

#else

    {
        constexpr variant<int, int, float, float, X, X> v( in_place_index_t<5>{}, 0, 0 );

        STATIC_ASSERT( v.index() == 5 );
    }

#endif
}
