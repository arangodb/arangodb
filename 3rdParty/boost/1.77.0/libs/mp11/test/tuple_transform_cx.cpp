// Copyright 2015, 2020 Peter Dimov
// Copyright 2020 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/tuple.hpp>
#include <boost/mp11/detail/config.hpp>

// Technically std::tuple isn't constexpr enabled in C++11, but it works with libstdc++

#if defined( BOOST_MP11_NO_CONSTEXPR ) || ( !defined(_MSC_VER) && !defined( __GLIBCXX__ ) && __cplusplus < 201400L ) || BOOST_MP11_WORKAROUND( BOOST_MP11_GCC, < 40800 )

int main() {}

#else

#include <tuple>
#include <utility>

constexpr int f( int x )
{
    return x + 1;
}

constexpr int g( int x, int y )
{
    return x + y + 1;
}

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

int main()
{
    {
        constexpr std::tuple<int, int, int> tp( 1, 2, 3 );

        constexpr auto r = boost::mp11::tuple_transform( f, tp );

        STATIC_ASSERT( r == std::make_tuple( 2, 3, 4 ) );
    }

    {
        constexpr std::tuple<int, int> tp1( 1, 2 );
        constexpr std::pair<int, int> tp2( 3, 4 );

        constexpr auto r = boost::mp11::tuple_transform( g, tp1, tp2 );

        STATIC_ASSERT( r == std::make_tuple( 5, 7 ) );
    }

#if defined( __clang_major__ ) && __clang_major__ == 3 && __clang_minor__ < 9
// "error: default initialization of an object of const type 'const std::tuple<>' without a user-provided default constructor"
#else

    {
        constexpr std::tuple<> tp;
        constexpr std::tuple<> r = boost::mp11::tuple_transform( f, tp );
        (void)r;
    }

#endif
}

#endif
