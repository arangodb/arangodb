
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
    X();
};

struct Y
{
    Y() = delete;
};

int main()
{
    {
        variant<int> v;

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 0 );
    }

    {
        variant<int const> v;

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 0 );
    }

    {
        variant<int, float, std::string> v;

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 0 );
    }

    {
        variant<int, int, std::string> v;

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 0 );
    }

    {
        variant<std::string> v;

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), std::string() );
    }

    {
        variant<std::string const> v;

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), std::string() );
    }

    {
        variant<std::string, int, float> v;

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), std::string() );
    }

    {
        variant<std::string, std::string, float> v;

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), std::string() );
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_nothrow_default_constructible<variant<int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_nothrow_default_constructible<variant<int const>>));
        BOOST_TEST_TRAIT_TRUE((std::is_nothrow_default_constructible<variant<int, X>>));
        BOOST_TEST_TRAIT_TRUE((std::is_nothrow_default_constructible<variant<int, float, X>>));
        BOOST_TEST_TRAIT_TRUE((std::is_nothrow_default_constructible<variant<int, int, X>>));
        BOOST_TEST_TRAIT_TRUE((std::is_nothrow_default_constructible<variant<int, X, X>>));

        BOOST_TEST_TRAIT_FALSE((std::is_nothrow_default_constructible<variant<X>>));
        BOOST_TEST_TRAIT_FALSE((std::is_nothrow_default_constructible<variant<X, int>>));
        BOOST_TEST_TRAIT_FALSE((std::is_nothrow_default_constructible<variant<X, int, float>>));

        BOOST_TEST_TRAIT_TRUE((std::is_default_constructible<variant<int, Y>>));
        BOOST_TEST_TRAIT_FALSE((std::is_default_constructible<variant<Y, int>>));
    }

    return boost::report_errors();
}
