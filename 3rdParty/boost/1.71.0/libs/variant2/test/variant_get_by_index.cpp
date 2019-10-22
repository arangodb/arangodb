
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

int main()
{
    {
        variant<int> v;

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<0>(v)), int&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<0>(std::move(v))), int&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<0>(&v)), int*>));
    }

    {
        variant<int> const v;

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<0>(v)), int const&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<0>(std::move(v))), int const&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<0>(&v)), int const*>));
    }

    {
        variant<int const> v;

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<0>(v)), int const&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<0>(std::move(v))), int const&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<0>(&v)), int const*>));
    }

    {
        variant<int volatile> const v;

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<0>(v)), int const volatile&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<0>(std::move(v))), int const volatile&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<0>(&v)), int const volatile*>));
    }

    {
        variant<int> v;

        BOOST_TEST_EQ( get<0>(v), 0 );
        BOOST_TEST_EQ( get_if<0>(&v), &get<0>(v) );

        BOOST_TEST_EQ( get<0>(std::move(v)), 0 );
    }

    {
        variant<int> const v;

        BOOST_TEST_EQ( get<0>(v), 0 );
        BOOST_TEST_EQ( get_if<0>(&v), &get<0>(v) );

        BOOST_TEST_EQ( get<0>(std::move(v)), 0 );
    }

    {
        variant<int> v( 1 );

        BOOST_TEST_EQ( get<0>(v), 1 );
        BOOST_TEST_EQ( get_if<0>(&v), &get<0>(v) );

        BOOST_TEST_EQ( get<0>(std::move(v)), 1 );
    }

    {
        variant<int> const v( 1 );

        BOOST_TEST_EQ( get<0>(v), 1 );
        BOOST_TEST_EQ( get_if<0>(&v), &get<0>(v) );

        BOOST_TEST_EQ( get<0>(std::move(v)), 1 );
    }

    {
        variant<int, float> v;

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<0>(v)), int&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<0>(std::move(v))), int&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<0>(&v)), int*>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<1>(v)), float&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<1>(std::move(v))), float&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<1>(&v)), float*>));
    }

    {
        variant<int, float> const v;

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<0>(v)), int const&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<0>(std::move(v))), int const&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<0>(&v)), int const*>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<1>(v)), float const&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<1>(std::move(v))), float const&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<1>(&v)), float const*>));
    }

    {
        variant<int const, float volatile> v;

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<0>(v)), int const&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<0>(std::move(v))), int const&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<0>(&v)), int const*>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<1>(v)), float volatile&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<1>(std::move(v))), float volatile&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<1>(&v)), float volatile*>));
    }

    {
        variant<int const, float volatile> const v;

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<0>(v)), int const&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<0>(std::move(v))), int const&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<0>(&v)), int const*>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<1>(v)), float const volatile&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<1>(std::move(v))), float const volatile&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<1>(&v)), float const volatile*>));
    }

    {
        variant<int, float> v;

        BOOST_TEST_EQ( get<0>(v), 0 );
        BOOST_TEST_EQ( get_if<0>(&v), &get<0>(v) );

        BOOST_TEST_THROWS( get<1>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<1>(&v), nullptr );

        BOOST_TEST_EQ( get<0>(std::move(v)), 0 );
    }

    {
        variant<int, float> const v;

        BOOST_TEST_EQ( get<0>(v), 0 );
        BOOST_TEST_EQ( get_if<0>(&v), &get<0>(v) );

        BOOST_TEST_THROWS( get<1>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<1>(&v), nullptr );

        BOOST_TEST_EQ( get<0>(std::move(v)), 0 );
    }

    {
        variant<int, float> v( 1 );

        BOOST_TEST_EQ( get<0>(v), 1 );
        BOOST_TEST_EQ( get_if<0>(&v), &get<0>(v) );

        BOOST_TEST_THROWS( get<1>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<1>(&v), nullptr );

        BOOST_TEST_EQ( get<0>(std::move(v)), 1 );
    }

    {
        variant<int, float> const v( 1 );

        BOOST_TEST_EQ( get<0>(v), 1 );
        BOOST_TEST_EQ( get_if<0>(&v), &get<0>(v) );

        BOOST_TEST_THROWS( get<1>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<1>(&v), nullptr );

        BOOST_TEST_EQ( get<0>(std::move(v)), 1 );
    }

    {
        variant<int, float> v( 3.14f );

        BOOST_TEST_THROWS( get<0>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<0>(&v), nullptr );

        BOOST_TEST_EQ( get<1>(v), 3.14f );
        BOOST_TEST_EQ( get_if<1>(&v), &get<1>(v) );

        BOOST_TEST_EQ( get<1>(std::move(v)), 3.14f );
    }

    {
        variant<int, float> const v( 3.14f );

        BOOST_TEST_THROWS( get<0>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<0>(&v), nullptr );

        BOOST_TEST_EQ( get<1>(v), 3.14f );
        BOOST_TEST_EQ( get_if<1>(&v), &get<1>(v) );

        BOOST_TEST_EQ( get<1>(std::move(v)), 3.14f );
    }

    {
        variant<int, float, float> v;

        BOOST_TEST_EQ( get<0>(v), 0 );
        BOOST_TEST_EQ( get_if<0>(&v), &get<0>(v) );

        BOOST_TEST_THROWS( get<1>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<1>(&v), nullptr );

        BOOST_TEST_THROWS( get<2>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<2>(&v), nullptr );

        BOOST_TEST_EQ( get<0>(std::move(v)), 0 );
    }

    {
        variant<int, float, float> const v;

        BOOST_TEST_EQ( get<0>(v), 0 );
        BOOST_TEST_EQ( get_if<0>(&v), &get<0>(v) );

        BOOST_TEST_THROWS( get<1>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<1>(&v), nullptr );

        BOOST_TEST_THROWS( get<2>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<2>(&v), nullptr );

        BOOST_TEST_EQ( get<0>(std::move(v)), 0 );
    }

    {
        variant<int, float, float> v( 1 );

        BOOST_TEST_EQ( get<0>(v), 1 );
        BOOST_TEST_EQ( get_if<0>(&v), &get<0>(v) );

        BOOST_TEST_THROWS( get<1>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<1>(&v), nullptr );

        BOOST_TEST_THROWS( get<2>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<2>(&v), nullptr );

        BOOST_TEST_EQ( get<0>(std::move(v)), 1 );
    }

    {
        variant<int, float, float> const v( 1 );

        BOOST_TEST_EQ( get<0>(v), 1 );
        BOOST_TEST_EQ( get_if<0>(&v), &get<0>(v) );

        BOOST_TEST_THROWS( get<1>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<1>(&v), nullptr );

        BOOST_TEST_THROWS( get<2>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<2>(&v), nullptr );

        BOOST_TEST_EQ( get<0>(std::move(v)), 1 );
    }

    {
        variant<int, int, float> v( 3.14f );

        BOOST_TEST_THROWS( get<0>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<0>(&v), nullptr );

        BOOST_TEST_THROWS( get<1>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<1>(&v), nullptr );

        BOOST_TEST_EQ( get<2>(v), 3.14f );
        BOOST_TEST_EQ( get_if<2>(&v), &get<2>(v) );

        BOOST_TEST_EQ( get<2>(std::move(v)), 3.14f );
    }

    {
        variant<int, int, float> const v( 3.14f );

        BOOST_TEST_THROWS( get<0>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<0>(&v), nullptr );

        BOOST_TEST_THROWS( get<1>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<1>(&v), nullptr );

        BOOST_TEST_EQ( get<2>(v), 3.14f );
        BOOST_TEST_EQ( get_if<2>(&v), &get<2>(v) );

        BOOST_TEST_EQ( get<2>(std::move(v)), 3.14f );
    }

    {
        variant<int> * p = 0;
        BOOST_TEST_EQ( get_if<0>(p), nullptr );
    }

    {
        variant<int const> * p = 0;
        BOOST_TEST_EQ( get_if<0>(p), nullptr );
    }

    {
        variant<int> const * p = 0;
        BOOST_TEST_EQ( get_if<0>(p), nullptr );
    }

    {
        variant<int const> const * p = 0;
        BOOST_TEST_EQ( get_if<0>(p), nullptr );
    }

    {
        variant<int, float> * p = 0;
        BOOST_TEST_EQ( get_if<0>(p), nullptr );
    }

    {
        variant<int, float> const * p = 0;
        BOOST_TEST_EQ( get_if<0>(p), nullptr );
    }

    {
        variant<int const, float volatile> * p = 0;
        BOOST_TEST_EQ( get_if<0>(p), nullptr );
    }

    {
        variant<int const, float volatile> const * p = 0;
        BOOST_TEST_EQ( get_if<0>(p), nullptr );
    }

    {
        variant<int, int, float, float> * p = 0;
        BOOST_TEST_EQ( get_if<0>(p), nullptr );
    }

    {
        variant<int, int, float, float> const * p = 0;
        BOOST_TEST_EQ( get_if<0>(p), nullptr );
    }

    return boost::report_errors();
}
