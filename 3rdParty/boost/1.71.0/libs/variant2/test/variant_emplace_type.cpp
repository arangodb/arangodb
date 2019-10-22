
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

struct Y1
{
};

struct Y2
{
    ~Y2() {}
};

struct Guard
{
    explicit Guard(int) {}
    Guard(Guard&&) = delete;
};

int main()
{
    {
        variant<int> v;
        BOOST_TEST_EQ( get<0>(v), 0 );

        v.emplace<int>( 1 );
        BOOST_TEST_EQ( get<0>(v), 1 );

        v.emplace<int>();
        BOOST_TEST_EQ( get<0>(v), 0 );
    }

    {
        variant<int, float> v;
        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 0 );

        v.emplace<int>( 1 );
        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 1 );

        v.emplace<float>( 3.14f );
        BOOST_TEST_EQ( v.index(), 1 );
        BOOST_TEST_EQ( get<1>(v), 3.14f );

        v.emplace<float>();
        BOOST_TEST_EQ( v.index(), 1 );
        BOOST_TEST_EQ( get<1>(v), 0 );

        v.emplace<int>();
        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 0 );
    }

    {
        variant<int, int, float, std::string> v;
        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 0 );

        v.emplace<float>( 3.14f );
        BOOST_TEST_EQ( v.index(), 2 );
        BOOST_TEST_EQ( get<2>(v), 3.14f );

        v.emplace<float>();
        BOOST_TEST_EQ( v.index(), 2 );
        BOOST_TEST_EQ( get<2>(v), 0 );

        v.emplace<std::string>( "s1" );
        BOOST_TEST_EQ( v.index(), 3 );
        BOOST_TEST_EQ( get<3>(v), std::string("s1") );

        v.emplace<std::string>( "s2" );
        BOOST_TEST_EQ( v.index(), 3 );
        BOOST_TEST_EQ( get<3>(v), std::string("s2") );

        v.emplace<std::string>();
        BOOST_TEST_EQ( v.index(), 3 );
        BOOST_TEST_EQ( get<3>(v), std::string() );

        v.emplace<std::string>( { 'a', 'b' } );
        BOOST_TEST_EQ( v.index(), 3 );
        BOOST_TEST_EQ( get<3>(v), (std::string{ 'a', 'b' }) );

        v.emplace<std::string>( { 'c', 'd' }, std::allocator<char>() );
        BOOST_TEST_EQ( v.index(), 3 );
        BOOST_TEST_EQ( get<3>(v), (std::string{ 'c', 'd' }) );
    }

    {
        variant<X1, X2> v;
        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v).v, 0 );

        v.emplace<X1>( 1 );
        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v).v, 1 );

        v.emplace<X2>( 2 );
        BOOST_TEST_EQ( v.index(), 1 );
        BOOST_TEST_EQ( get<1>(v).v, 2 );

        v.emplace<X1>();
        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v).v, 0 );

        v.emplace<X1>( 4 );
        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v).v, 4 );

        v.emplace<X2>();

        BOOST_TEST_EQ( v.index(), 1 );
        BOOST_TEST_EQ( get<1>(v).v, 0 );

        v.emplace<X2>( 6 );
        BOOST_TEST_EQ( v.index(), 1 );
        BOOST_TEST_EQ( get<1>(v).v, 6 );

        v.emplace<X1>( 3 );
        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v).v, 3 );

        v.emplace<X1>( 4 );
        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v).v, 4 );
    }

    {
        variant<Y1, Guard> v;
        v.emplace<Guard>( 1 );
    }

    {
        variant<Y2, Guard> v;
        v.emplace<Guard>( 1 );
    }

    return boost::report_errors();
}
