
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>

using namespace boost::variant2;

int main()
{
    {
        variant<int> v;
        BOOST_TEST( holds_alternative<int>( v ) );
    }

    {
        variant<int, float> v;
        BOOST_TEST( holds_alternative<int>( v ) );
        BOOST_TEST( !holds_alternative<float>( v ) );
    }

    {
        variant<int, float> v( 3.14f );
        BOOST_TEST( !holds_alternative<int>( v ) );
        BOOST_TEST( holds_alternative<float>( v ) );
    }

    {
        variant<int, float, float> v;
        BOOST_TEST( holds_alternative<int>( v ) );
    }

    {
        variant<int, int, float> v( 3.14f );
        BOOST_TEST( holds_alternative<float>( v ) );
    }

    {
        variant<int, float, std::string> v;
        BOOST_TEST( holds_alternative<int>( v ) );
        BOOST_TEST( !holds_alternative<float>( v ) );
        BOOST_TEST( !holds_alternative<std::string>( v ) );
    }

    {
        variant<int, float, std::string> v( 3.14f );
        BOOST_TEST( !holds_alternative<int>( v ) );
        BOOST_TEST( holds_alternative<float>( v ) );
        BOOST_TEST( !holds_alternative<std::string>( v ) );
    }

    {
        variant<int, float, std::string> v( "text" );
        BOOST_TEST( !holds_alternative<int>( v ) );
        BOOST_TEST( !holds_alternative<float>( v ) );
        BOOST_TEST( holds_alternative<std::string>( v ) );
    }

    {
        variant<int, int, float, std::string> v( 3.14f );
        BOOST_TEST( holds_alternative<float>( v ) );
        BOOST_TEST( !holds_alternative<std::string>( v ) );
    }

    {
        variant<int, int, float, std::string> v( "text" );
        BOOST_TEST( !holds_alternative<float>( v ) );
        BOOST_TEST( holds_alternative<std::string>( v ) );
    }

    return boost::report_errors();
}
