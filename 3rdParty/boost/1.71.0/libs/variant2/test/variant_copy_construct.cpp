
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
    X1() {}
    X1(X1 const&) {}
    X1(X1&&) {}
};

inline bool operator==( X1, X1 ) { return true; }

STATIC_ASSERT( !std::is_nothrow_default_constructible<X1>::value );
STATIC_ASSERT( !std::is_nothrow_copy_constructible<X1>::value );
STATIC_ASSERT( !std::is_nothrow_move_constructible<X1>::value );

struct X2
{
    X2() {}
    X2(X2 const&) {}
    X2(X2&&) {}
};

inline bool operator==( X2, X2 ) { return true; }

STATIC_ASSERT( !std::is_nothrow_default_constructible<X2>::value );
STATIC_ASSERT( !std::is_nothrow_copy_constructible<X2>::value );
STATIC_ASSERT( !std::is_nothrow_move_constructible<X2>::value );

struct Y
{
    Y( Y const& ) = delete;
};

template<class V> static void test( V const & v )
{
    V v2( v );

    BOOST_TEST_EQ( v.index(), v2.index() );
    BOOST_TEST( v == v2 );
}

int main()
{
    test( variant<int>() );
    test( variant<int>(1) );

    test( variant<int const>() );
    test( variant<int const>(1) );

    test( variant<int, float>() );
    test( variant<int, float>(1) );
    test( variant<int, float>(3.14f) );

    test( variant<int const, float const>() );
    test( variant<int const, float const>(1) );
    test( variant<int const, float const>(3.14f) );

    test( variant<std::string>() );
    test( variant<std::string>("test") );

    test( variant<std::string const>() );
    test( variant<std::string const>("test") );

    test( variant<int, float, std::string>() );
    test( variant<int, float, std::string>(1) );
    test( variant<int, float, std::string>(3.14f) );
    test( variant<int, float, std::string>("test") );

    test( variant<int, int>() );

    test( variant<int, int, float>() );
    test( variant<int, int, float>(3.14f) );

    test( variant<int, int, float, float>() );

    test( variant<int, int, float, float, std::string>("test") );

    test( variant<std::string, std::string, float>() );

    test( variant<X1 const>() );

    test( variant<X1, X2>() );
    test( variant<X1, X2, int>() );
    test( variant<X1, X2, X2>() );
    test( variant<X1, X1, X2, X2>() );

    {
        variant<X1, X2> v;
        v.emplace<X2>();

        test( v );
    }

    {
        variant<X1, X1, X2> v;
        v.emplace<X2>();

        test( v );
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_nothrow_copy_constructible<variant<int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_nothrow_copy_constructible<variant<int const>>));
        BOOST_TEST_TRAIT_TRUE((std::is_nothrow_copy_constructible<variant<int, int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_nothrow_copy_constructible<variant<int, float>>));
        BOOST_TEST_TRAIT_TRUE((std::is_nothrow_copy_constructible<variant<int, int, float, float>>));

        BOOST_TEST_TRAIT_FALSE((std::is_nothrow_copy_constructible<variant<X1>>));
        BOOST_TEST_TRAIT_FALSE((std::is_nothrow_copy_constructible<variant<X1, int>>));
        BOOST_TEST_TRAIT_FALSE((std::is_nothrow_copy_constructible<variant<X1, int, float>>));

        BOOST_TEST_TRAIT_FALSE((std::is_nothrow_copy_constructible<variant<int, X1>>));
        BOOST_TEST_TRAIT_FALSE((std::is_nothrow_copy_constructible<variant<int, int, X1>>));

        BOOST_TEST_TRAIT_FALSE((std::is_nothrow_copy_constructible<variant<X1, X2>>));
        BOOST_TEST_TRAIT_FALSE((std::is_nothrow_copy_constructible<variant<X1, X2, int, int>>));

        BOOST_TEST_TRAIT_TRUE((std::is_copy_constructible<variant<X1, X2>>));

#if !BOOST_WORKAROUND( BOOST_MSVC, <= 1910 )

        BOOST_TEST_TRAIT_FALSE((std::is_copy_constructible<variant<int, float, Y>>));

#endif
    }

    return boost::report_errors();
}
