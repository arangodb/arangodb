
// Copyright 2015 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#if defined(_MSC_VER)
#pragma warning( disable: 4244 ) // 'initializing': conversion from 'int' to 'char', possible loss of data
#endif

#include <boost/mp11/tuple.hpp>
#include <boost/config.hpp>

// Technically std::tuple isn't constexpr enabled in C++11, but it works with libstdc++

#if defined( BOOST_NO_CXX11_CONSTEXPR ) || ( !defined( __GLIBCXX__ ) && __cplusplus < 201400L )

int main() {}

#else

#include <tuple>
#include <array>
#include <utility>

constexpr int f( int x, int y, int z )
{
    return x * 100 + y * 10 + z;
}

constexpr int g( int x, int y )
{
    return x * 10 + y;
}

constexpr int h()
{
    return 11;
}

int main()
{
    {
        constexpr std::tuple<int, short, char> tp{ 1, 2, 3 };
        constexpr auto r = boost::mp11::tuple_apply( f, tp );
        static_assert( r == 123, "r == 123" );
    }

    {
        constexpr std::pair<short, char> tp{ 1, 2 };
        constexpr auto r = boost::mp11::tuple_apply( g, tp );
        static_assert( r == 12, "r == 12" );
    }

    {
        constexpr std::array<short, 3> tp{{ 1, 2, 3 }};
        constexpr auto r = boost::mp11::tuple_apply( f, tp );
        static_assert( r == 123, "r == 123" );
    }

#if defined( __clang_major__ ) && __clang_major__ == 3 && __clang_minor__ < 9
// "error: default initialization of an object of const type 'const std::tuple<>' without a user-provided default constructor"
#else

    {
        constexpr std::tuple<> tp;
        constexpr auto r = boost::mp11::tuple_apply( h, tp );
        static_assert( r == 11, "r == 11" );
    }

#endif
}

#endif
