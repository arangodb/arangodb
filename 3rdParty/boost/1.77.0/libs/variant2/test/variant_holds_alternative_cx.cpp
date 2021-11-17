
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
        STATIC_ASSERT( holds_alternative<int>( v ) );
    }

    {
        constexpr variant<int, float> v;
        STATIC_ASSERT( holds_alternative<int>( v ) );
        STATIC_ASSERT( !holds_alternative<float>( v ) );
    }

    {
        constexpr variant<int, float> v( 3.14f );
        STATIC_ASSERT( !holds_alternative<int>( v ) );
        STATIC_ASSERT( holds_alternative<float>( v ) );
    }

    {
        constexpr variant<int, float, float> v;
        STATIC_ASSERT( holds_alternative<int>( v ) );
    }

    {
        constexpr variant<int, int, float> v( 3.14f );
        STATIC_ASSERT( holds_alternative<float>( v ) );
    }
}
