
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>

using namespace boost::variant2;

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

int main()
{
    {
        constexpr variant<int> v;

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( get<0>(v) == 0 );
    }

    {
        constexpr variant<int const> v;

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( get<0>(v) == 0 );
    }

    {
        constexpr variant<int, float> v;

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( get<0>(v) == 0 );
    }

    {
        constexpr variant<int, int, float> v;

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( get<0>(v) == 0 );
    }

    {
        constexpr variant<int, float, float> v;

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( get<0>(v) == 0 );
    }
}
