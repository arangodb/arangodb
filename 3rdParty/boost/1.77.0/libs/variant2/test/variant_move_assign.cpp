
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <utility>
#include <string>

using namespace boost::variant2;

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

struct X1
{
    int v;

    X1(): v(0) {}
    explicit X1(int v): v(v) {}
    X1(X1 const& r): v(r.v) {}
    X1(X1&& r): v(r.v) {}
    X1& operator=( X1 const& r ) { v = r.v; return *this; }
    X1& operator=( X1&& r ) { v = r.v; return *this; }
};

inline bool operator==( X1 const& a, X1 const& b ) { return a.v == b.v; }

STATIC_ASSERT( !std::is_nothrow_default_constructible<X1>::value );
STATIC_ASSERT( !std::is_nothrow_copy_constructible<X1>::value );
STATIC_ASSERT( !std::is_nothrow_move_constructible<X1>::value );
STATIC_ASSERT( !std::is_nothrow_copy_assignable<X1>::value );
STATIC_ASSERT( !std::is_nothrow_move_assignable<X1>::value );

struct X2
{
    int v;

    X2(): v(0) {}
    explicit X2(int v): v(v) {}
    X2(X2 const& r): v(r.v) {}
    X2(X2&& r): v(r.v) {}
    X2& operator=( X2 const& r ) { v = r.v; return *this; }
    X2& operator=( X2&& r ) { v = r.v; return *this; }
};

inline bool operator==( X2 const& a, X2 const& b ) { return a.v == b.v; }

STATIC_ASSERT( !std::is_nothrow_default_constructible<X2>::value );
STATIC_ASSERT( !std::is_nothrow_copy_constructible<X2>::value );
STATIC_ASSERT( !std::is_nothrow_move_constructible<X2>::value );
STATIC_ASSERT( !std::is_nothrow_copy_assignable<X2>::value );
STATIC_ASSERT( !std::is_nothrow_move_assignable<X2>::value );

struct Y
{
    Y& operator=( Y&& ) = delete;
};

int main()
{
    {
        variant<int> v;
        BOOST_TEST_EQ( get<0>(v), 0 );

        variant<int> v2( 1 );

        v = std::move(v2);
        BOOST_TEST_EQ( get<0>(v), 1 );

        variant<int> v3( 2 );

        v = std::move(v3);
        BOOST_TEST_EQ( get<0>(v), 2 );
    }

    {
        variant<int, float> v;
        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 0 );

        variant<int, float> v2( 1 );

        v = std::move(v2);
        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 1 );

        variant<int, float> v3( 3.14f );

        v = std::move(v3);
        BOOST_TEST_EQ( v.index(), 1 );
        BOOST_TEST_EQ( get<1>(v), 3.14f );

        variant<int, float> v4( 3.15f );

        v = std::move(v4);
        BOOST_TEST_EQ( v.index(), 1 );
        BOOST_TEST_EQ( get<1>(v), 3.15f );
    }

    {
        variant<int, int, float, std::string> v;
        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 0 );

        variant<int, int, float, std::string> v2( in_place_index_t<1>{}, 1 );

        v = std::move(v2);
        BOOST_TEST_EQ( v.index(), 1 );
        BOOST_TEST_EQ( get<1>(v), 1 );

        variant<int, int, float, std::string> v3( 3.14f );

        v = std::move(v3);
        BOOST_TEST_EQ( v.index(), 2 );
        BOOST_TEST_EQ( get<2>(v), 3.14f );

        variant<int, int, float, std::string> v4( 3.15f );

        v = std::move(v4);
        BOOST_TEST_EQ( v.index(), 2 );
        BOOST_TEST_EQ( get<2>(v), 3.15f );

        variant<int, int, float, std::string> v5( "s1" );

        v = std::move(v5);
        BOOST_TEST_EQ( v.index(), 3 );
        BOOST_TEST_EQ( get<3>(v), std::string("s1") );

        variant<int, int, float, std::string> v6( "s2" );

        v = std::move(v6);
        BOOST_TEST_EQ( v.index(), 3 );
        BOOST_TEST_EQ( get<3>(v), std::string("s2") );
    }

    {
        variant<X1, X2> v;
        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v).v, 0 );

        variant<X1, X2> v2( X1{1} );

        v = std::move(v2);
        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v).v, 1 );

        variant<X1, X2> v3( in_place_index_t<1>{}, 2 );

        v = std::move(v3);
        BOOST_TEST_EQ( v.index(), 1 );
        BOOST_TEST_EQ( get<1>(v).v, 2 );

        variant<X1, X2> v4( in_place_index_t<1>{}, 3 );

        v = std::move(v4);
        BOOST_TEST_EQ( v.index(), 1 );
        BOOST_TEST_EQ( get<1>(v).v, 3 );

        variant<X1, X2> v5( in_place_index_t<0>{}, 4 );

        v = std::move(v5);
        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v).v, 4 );
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_nothrow_move_assignable<variant<int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_nothrow_move_assignable<variant<int, int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_nothrow_move_assignable<variant<int, float>>));
        BOOST_TEST_TRAIT_TRUE((std::is_nothrow_move_assignable<variant<int, int, float, float>>));

        BOOST_TEST_TRAIT_FALSE((std::is_nothrow_move_assignable<variant<X1>>));
        BOOST_TEST_TRAIT_FALSE((std::is_nothrow_move_assignable<variant<X1, int>>));
        BOOST_TEST_TRAIT_FALSE((std::is_nothrow_move_assignable<variant<X1, int, float>>));

        BOOST_TEST_TRAIT_FALSE((std::is_nothrow_move_assignable<variant<int, X1>>));
        BOOST_TEST_TRAIT_FALSE((std::is_nothrow_move_assignable<variant<int, int, X1>>));

        BOOST_TEST_TRAIT_FALSE((std::is_nothrow_move_assignable<variant<X1, X2>>));
        BOOST_TEST_TRAIT_FALSE((std::is_nothrow_move_assignable<variant<X1, X2, int, int>>));

        BOOST_TEST_TRAIT_TRUE((std::is_move_assignable<variant<X1, X2>>));
        BOOST_TEST_TRAIT_FALSE((std::is_move_assignable<variant<int const>>));
        BOOST_TEST_TRAIT_FALSE((std::is_move_assignable<variant<int, float, Y>>));
    }

    return boost::report_errors();
}
