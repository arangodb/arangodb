
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

int main()
{
    {
        variant<int> v( 1 );

        variant<int, float> v2( v );

        BOOST_TEST( holds_alternative<int>( v2 ) );
        BOOST_TEST_EQ( get<int>( v ), get<int>( v2 ) );

        variant<int, float> v3( std::move(v) );

        BOOST_TEST( holds_alternative<int>( v3 ) );
        BOOST_TEST_EQ( get<int>( v2 ), get<int>( v3 ) );
    }

    {
        variant<int> const v( 1 );

        variant<int, float> v2( v );

        BOOST_TEST( holds_alternative<int>( v2 ) );
        BOOST_TEST_EQ( get<int>( v ), get<int>( v2 ) );

        variant<int, float> v3( std::move(v) );

        BOOST_TEST( holds_alternative<int>( v3 ) );
        BOOST_TEST_EQ( get<int>( v2 ), get<int>( v3 ) );
    }

    {
        variant<int const> v( 1 );

        variant<int const, float> v2( v );

        BOOST_TEST( holds_alternative<int const>( v2 ) );
        BOOST_TEST_EQ( get<int const>( v ), get<int const>( v2 ) );

        variant<int const, float> v3( std::move(v) );

        BOOST_TEST( holds_alternative<int const>( v3 ) );
        BOOST_TEST_EQ( get<int const>( v2 ), get<int const>( v3 ) );
    }

    {
        variant<int const> const v( 1 );

        variant<int const, float> v2( v );

        BOOST_TEST( holds_alternative<int const>( v2 ) );
        BOOST_TEST_EQ( get<int const>( v ), get<int const>( v2 ) );

        variant<int const, float> v3( std::move(v) );

        BOOST_TEST( holds_alternative<int const>( v3 ) );
        BOOST_TEST_EQ( get<int const>( v2 ), get<int const>( v3 ) );
    }

    {
        variant<float> v( 3.14f );

        variant<int, float> v2( v );

        BOOST_TEST( holds_alternative<float>( v2 ) );
        BOOST_TEST_EQ( get<float>( v ), get<float>( v2 ) );

        variant<int, float> v3( std::move(v) );

        BOOST_TEST( holds_alternative<float>( v3 ) );
        BOOST_TEST_EQ( get<float>( v2 ), get<float>( v3 ) );
    }

    {
        variant<float> v( 3.15f );

        variant<int, int, float> v2( v );

        BOOST_TEST( holds_alternative<float>( v2 ) );
        BOOST_TEST_EQ( get<float>( v ), get<float>( v2 ) );

        variant<int, int, float> v3( std::move(v) );

        BOOST_TEST( holds_alternative<float>( v3 ) );
        BOOST_TEST_EQ( get<float>( v2 ), get<float>( v3 ) );
    }

    {
        variant<float, std::string> v( "s1" );

        variant<int, int, float, std::string> v2( v );

        BOOST_TEST( holds_alternative<std::string>( v2 ) );
        BOOST_TEST_EQ( get<std::string>( v ), get<std::string>( v2 ) );

        variant<int, int, float, std::string> v3( std::move(v) );

        BOOST_TEST( holds_alternative<std::string>( v3 ) );
        BOOST_TEST_EQ( get<std::string>( v2 ), get<std::string>( v3 ) );
    }

    {
        variant<X1, X2> v{ X1{1} };

        variant<int, int, float, float, X1, X2> v2( v );

        BOOST_TEST( holds_alternative<X1>( v2 ) );
        BOOST_TEST_EQ( get<X1>( v ).v, get<X1>( v2 ).v );

        variant<int, int, float, float, X1, X2> v3( std::move(v) );

        BOOST_TEST( holds_alternative<X1>( v3 ) );
        BOOST_TEST_EQ( get<X1>( v2 ).v, get<X1>( v3 ).v );
    }

    return boost::report_errors();
}
