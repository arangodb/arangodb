
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#if defined(_MSC_VER)
# pragma warning( disable: 4702 ) // unreachable code
#endif

#include <boost/variant2/variant.hpp>
#include <boost/core/lightweight_test.hpp>
#include <type_traits>
#include <utility>
#include <string>
#include <stdexcept>

using namespace boost::variant2;
namespace v2d = boost::variant2::detail;

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

//

enum E1 { e1 };
enum E1x { e1x };

struct X1
{
    X1() = default;

    X1( E1 ) noexcept {}
    X1( E1x ) { throw std::runtime_error( "X1(E1x)" ); }
};

STATIC_ASSERT( std::is_nothrow_default_constructible<X1>::value );
STATIC_ASSERT( std::is_nothrow_copy_constructible<X1>::value );
STATIC_ASSERT( std::is_nothrow_move_constructible<X1>::value );
STATIC_ASSERT( std::is_trivially_destructible<X1>::value );
STATIC_ASSERT( v2d::is_trivially_move_assignable<X1>::value );
STATIC_ASSERT( std::is_nothrow_constructible<X1, E1>::value );
STATIC_ASSERT( !std::is_nothrow_constructible<X1, E1x>::value );

enum E2 { e2 };
enum E2x { e2x };

struct X2
{
    X2();
    ~X2();

    X2( E2 ) noexcept {}
    X2( E2x ) { throw std::runtime_error( "X2(E2x)" ); }
};

X2::X2() {}
X2::~X2() {}

STATIC_ASSERT( !std::is_nothrow_default_constructible<X2>::value );
STATIC_ASSERT( std::is_nothrow_copy_constructible<X2>::value );
STATIC_ASSERT( std::is_nothrow_move_constructible<X2>::value );
STATIC_ASSERT( !std::is_trivially_destructible<X2>::value );
STATIC_ASSERT( std::is_nothrow_constructible<X2, E2>::value );
STATIC_ASSERT( !std::is_nothrow_constructible<X2, E2x>::value );

enum E3 { e3 };
enum E3x { e3x };

struct X3
{
    X3();

    X3( X3 const& ) {}
    X3( X3&& ) {}

    X3( E3 ) noexcept {}
    X3( E3x ) { throw std::runtime_error( "X3(E3x)" ); }

    X3& operator=( X3 const& ) = default;
    X3& operator=( X3&& ) = default;
};

X3::X3() {}

STATIC_ASSERT( !std::is_nothrow_default_constructible<X3>::value );
STATIC_ASSERT( !std::is_nothrow_copy_constructible<X3>::value );
STATIC_ASSERT( !std::is_nothrow_move_constructible<X3>::value );
STATIC_ASSERT( std::is_trivially_destructible<X3>::value );
//STATIC_ASSERT( v2d::is_trivially_move_assignable<X3>::value );
STATIC_ASSERT( std::is_nothrow_constructible<X3, E3>::value );
STATIC_ASSERT( !std::is_nothrow_constructible<X3, E3x>::value );

//

STATIC_ASSERT( std::is_nothrow_default_constructible<monostate>::value );
STATIC_ASSERT( std::is_nothrow_copy_constructible<monostate>::value );
STATIC_ASSERT( std::is_nothrow_move_constructible<monostate>::value );
STATIC_ASSERT( std::is_trivially_destructible<monostate>::value );

//

int main()
{
    {
        variant<X2, X1> v;

        BOOST_TEST_EQ( v.index(), 0 );

        try
        {
            v = e1x;
            BOOST_ERROR( "`v = e1x;` failed to throw" );
        }
        catch( std::exception const& )
        {
            // strong guarantee
            BOOST_TEST_EQ( v.index(), 0 );
        }
    }

    {
        variant<X1, X2> v( e2 );

        BOOST_TEST_EQ( v.index(), 1 );

        try
        {
            v = e1x;
            BOOST_ERROR( "`v = e1x;` failed to throw" );
        }
        catch( std::exception const& )
        {
            // strong guarantee
            BOOST_TEST_EQ( v.index(), 1 );
        }
    }

    {
        variant<X2, X1, monostate> v;

        BOOST_TEST_EQ( v.index(), 0 );

        try
        {
            v = e1x;
            BOOST_ERROR( "`v = e1x;` failed to throw" );
        }
        catch( std::exception const& )
        {
            // strong guarantee
            BOOST_TEST_EQ( v.index(), 0 );
        }
    }

    {
        variant<X1, X2, monostate> v( e2 );

        BOOST_TEST_EQ( v.index(), 1 );

        try
        {
            v = e1x;
            BOOST_ERROR( "`v = e1x;` failed to throw" );
        }
        catch( std::exception const& )
        {
            // strong guarantee
            BOOST_TEST_EQ( v.index(), 1 );
        }
    }

    {
        variant<X2, X3, X1> v;

        BOOST_TEST_EQ( v.index(), 0 );

        try
        {
            v = e3x;
            BOOST_ERROR( "`v = e3x;` failed to throw" );
        }
        catch( std::exception const& )
        {
            // strong guarantee
            BOOST_TEST_EQ( v.index(), 0 );
        }
    }

    {
        variant<X2, X3, X1, monostate> v;

        BOOST_TEST_EQ( v.index(), 0 );

        try
        {
            v = e3x;
            BOOST_ERROR( "`v = e3x;` failed to throw" );
        }
        catch( std::exception const& )
        {
            // strong guarantee
            BOOST_TEST_EQ( v.index(), 0 );
        }
    }

    {
        variant<X2, X3> v;

        BOOST_TEST_EQ( v.index(), 0 );

        try
        {
            v = e3x;
            BOOST_ERROR( "`v = e3x;` failed to throw" );
        }
        catch( std::exception const& )
        {
            // double buffered, no change
            BOOST_TEST_EQ( v.index(), 0 );
        }
    }

    {
        variant<X3, X1> v;

        BOOST_TEST_EQ( v.index(), 0 );

        try
        {
            v = e1x;
            BOOST_ERROR( "`v = e1x;` failed to throw" );
        }
        catch( std::exception const& )
        {
            // strong guarantee
            BOOST_TEST_EQ( v.index(), 0 );
        }
    }

    {
        variant<X3, X1, monostate> v;

        BOOST_TEST_EQ( v.index(), 0 );

        try
        {
            v = e1x;
            BOOST_ERROR( "`v = e1x;` failed to throw" );
        }
        catch( std::exception const& )
        {
            // strong guarantee
            BOOST_TEST_EQ( v.index(), 0 );
        }
    }

    return boost::report_errors();
}
