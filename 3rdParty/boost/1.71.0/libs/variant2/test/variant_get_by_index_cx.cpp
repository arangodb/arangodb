
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>
#include <boost/config.hpp>

using namespace boost::variant2;

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

#if BOOST_WORKAROUND(BOOST_GCC, < 50000)
# define STATIC_ASSERT_IF(...)
#else
# define STATIC_ASSERT_IF(...) static_assert(__VA_ARGS__, #__VA_ARGS__)
#endif

int main()
{
    {
        constexpr variant<int> v;

        STATIC_ASSERT( get<0>(v) == 0 );

        STATIC_ASSERT_IF( get_if<0>(&v) == &get<0>(v) );
    }

    {
        constexpr variant<int> v( 1 );

        STATIC_ASSERT( get<0>(v) == 1 );

        STATIC_ASSERT_IF( get_if<0>(&v) == &get<0>(v) );
    }

    {
        constexpr variant<int, float> v;

        STATIC_ASSERT( get<0>(v) == 0 );

        STATIC_ASSERT_IF( get_if<0>(&v) == &get<0>(v) );
        STATIC_ASSERT_IF( get_if<1>(&v) == nullptr );
    }

    {
        constexpr variant<int, float> v( 1 );

        STATIC_ASSERT( get<0>(v) == 1 );

        STATIC_ASSERT_IF( get_if<0>(&v) == &get<0>(v) );
        STATIC_ASSERT_IF( get_if<1>(&v) == nullptr );
    }

    {
        constexpr variant<int, float> v( 3.14f );

        STATIC_ASSERT( get<1>(v) == 3.14f );

        STATIC_ASSERT_IF( get_if<0>(&v) == nullptr );
        STATIC_ASSERT_IF( get_if<1>(&v) == &get<1>(v) );
    }

    {
        constexpr variant<int, float, float> v;

        STATIC_ASSERT( get<0>(v) == 0 );

        STATIC_ASSERT_IF( get_if<0>(&v) == &get<0>(v) );
        STATIC_ASSERT_IF( get_if<1>(&v) == nullptr );
        STATIC_ASSERT_IF( get_if<2>(&v) == nullptr );
    }

    {
        constexpr variant<int, float, float> v( 1 );

        STATIC_ASSERT( get<0>(v) == 1 );

        STATIC_ASSERT_IF( get_if<0>(&v) == &get<0>(v) );
        STATIC_ASSERT_IF( get_if<1>(&v) == nullptr );
        STATIC_ASSERT_IF( get_if<2>(&v) == nullptr );
    }

    {
        constexpr variant<int, int, float> v( 3.14f );

        STATIC_ASSERT( get<2>(v) == 3.14f );

        STATIC_ASSERT_IF( get_if<0>(&v) == nullptr );
        STATIC_ASSERT_IF( get_if<1>(&v) == nullptr );
        STATIC_ASSERT_IF( get_if<2>(&v) == &get<2>(v) );
    }
}
