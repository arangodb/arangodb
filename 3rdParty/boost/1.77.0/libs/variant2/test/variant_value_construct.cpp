
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

struct X
{
    operator int() const { return 2; }
};

int main()
{
    {
        variant<int> v( 1 );

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 1 );
    }

    {
        variant<int> v( 'a' );

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 'a' );
    }

    {
        variant<int> v( X{} );

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 2 );
    }

    {
        variant<int const> v( 1 );

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 1 );
    }

    {
        variant<int const> v( 'a' );

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 'a' );
    }

    {
        variant<int const> v( X{} );

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 2 );
    }

    {
        variant<int, float, std::string> v( 1 );

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST( holds_alternative<int>(v) );
        BOOST_TEST_EQ( get<0>(v), 1 );
    }

    {
        variant<int, float, std::string> v( 'a' );

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST( holds_alternative<int>(v) );
        BOOST_TEST_EQ( get<0>(v), 'a' );
    }

    {
        variant<int, float, std::string> v( X{} );

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST( holds_alternative<int>(v) );
        BOOST_TEST_EQ( get<0>(v), 2 );
    }

    {
        variant<int, float, std::string> v( 3.14f );

        BOOST_TEST_EQ( v.index(), 1 );
        BOOST_TEST( holds_alternative<float>(v) );
        BOOST_TEST_EQ( get<1>(v), 3.14f );
    }

    {
        variant<int, float, std::string> v( "text" );

        BOOST_TEST_EQ( v.index(), 2 );
        BOOST_TEST( holds_alternative<std::string>(v) );
        BOOST_TEST_EQ( get<2>(v), std::string("text") );
    }

    {
        variant<int, int, float, std::string> v( 3.14f );

        BOOST_TEST_EQ( v.index(), 2 );
        BOOST_TEST( holds_alternative<float>(v) );
        BOOST_TEST_EQ( get<2>(v), 3.14f );
    }

    {
        variant<int, int, float, std::string> v( "text" );

        BOOST_TEST_EQ( v.index(), 3 );
        BOOST_TEST( holds_alternative<std::string>(v) );
        BOOST_TEST_EQ( get<3>(v), std::string("text") );
    }

    return boost::report_errors();
}
