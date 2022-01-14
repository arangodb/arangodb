
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#if defined( __clang__ ) && defined( __has_warning )
# if __has_warning( "-Wdeprecated-volatile" )
#  pragma clang diagnostic ignored "-Wdeprecated-volatile"
# endif
#endif

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

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<int>(v)), int&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<int>(std::move(v))), int&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<int>(&v)), int*>));
    }

    {
        variant<int> const v;

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<int>(v)), int const&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<int>(std::move(v))), int const&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<int>(&v)), int const*>));
    }

    {
        variant<int const> v;

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<int const>(v)), int const&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<int const>(std::move(v))), int const&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<int const>(&v)), int const*>));
    }

    {
        variant<int volatile> const v;

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<int volatile>(v)), int const volatile&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<int volatile>(std::move(v))), int const volatile&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<int volatile>(&v)), int const volatile*>));
    }

    {
        variant<int> v;

        BOOST_TEST_EQ( get<int>(v), 0 );
        BOOST_TEST_EQ( get_if<int>(&v), &get<int>(v) );

        BOOST_TEST_EQ( get<int>(std::move(v)), 0 );
    }

    {
        variant<int> const v;

        BOOST_TEST_EQ( get<int>(v), 0 );
        BOOST_TEST_EQ( get_if<int>(&v), &get<int>(v) );

        BOOST_TEST_EQ( get<int>(std::move(v)), 0 );
    }

    {
        variant<int> v( 1 );

        BOOST_TEST_EQ( get<int>(v), 1 );
        BOOST_TEST_EQ( get_if<int>(&v), &get<int>(v) );

        BOOST_TEST_EQ( get<int>(std::move(v)), 1 );
    }

    {
        variant<int> const v( 1 );

        BOOST_TEST_EQ( get<int>(v), 1 );
        BOOST_TEST_EQ( get_if<int>(&v), &get<int>(v) );

        BOOST_TEST_EQ( get<int>(std::move(v)), 1 );
    }

    {
        variant<int, float> v;

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<int>(v)), int&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<int>(std::move(v))), int&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<int>(&v)), int*>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<float>(v)), float&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<float>(std::move(v))), float&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<float>(&v)), float*>));
    }

    {
        variant<int, float> const v;

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<int>(v)), int const&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<int>(std::move(v))), int const&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<int>(&v)), int const*>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<float>(v)), float const&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<float>(std::move(v))), float const&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<float>(&v)), float const*>));
    }

    {
        variant<int const, float volatile> v;

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<int const>(v)), int const&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<int const>(std::move(v))), int const&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<int const>(&v)), int const*>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<float volatile>(v)), float volatile&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<float volatile>(std::move(v))), float volatile&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<float volatile>(&v)), float volatile*>));
    }

    {
        variant<int const, float volatile> const v;

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<int const>(v)), int const&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<int const>(std::move(v))), int const&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<int const>(&v)), int const*>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<float volatile>(v)), float const volatile&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get<float volatile>(std::move(v))), float const volatile&&>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(get_if<float volatile>(&v)), float const volatile*>));
    }

    {
        variant<int, float> v;

        BOOST_TEST_EQ( get<int>(v), 0 );
        BOOST_TEST_EQ( get_if<int>(&v), &get<int>(v) );

        BOOST_TEST_THROWS( get<float>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<float>(&v), nullptr );

        BOOST_TEST_EQ( get<int>(std::move(v)), 0 );
    }

    {
        variant<int, float> const v;

        BOOST_TEST_EQ( get<int>(v), 0 );
        BOOST_TEST_EQ( get_if<int>(&v), &get<int>(v) );

        BOOST_TEST_THROWS( get<float>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<float>(&v), nullptr );

        BOOST_TEST_EQ( get<int>(std::move(v)), 0 );
    }

    {
        variant<int, float> v( 1 );

        BOOST_TEST_EQ( get<int>(v), 1 );
        BOOST_TEST_EQ( get_if<int>(&v), &get<int>(v) );

        BOOST_TEST_THROWS( get<float>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<float>(&v), nullptr );

        BOOST_TEST_EQ( get<int>(std::move(v)), 1 );
    }

    {
        variant<int, float> const v( 1 );

        BOOST_TEST_EQ( get<int>(v), 1 );
        BOOST_TEST_EQ( get_if<int>(&v), &get<int>(v) );

        BOOST_TEST_THROWS( get<float>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<float>(&v), nullptr );

        BOOST_TEST_EQ( get<int>(std::move(v)), 1 );
    }

    {
        variant<int, float> v( 3.14f );

        BOOST_TEST_THROWS( get<int>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<int>(&v), nullptr );

        BOOST_TEST_EQ( get<float>(v), 3.14f );
        BOOST_TEST_EQ( get_if<float>(&v), &get<float>(v) );

        BOOST_TEST_EQ( get<float>(std::move(v)), 3.14f );
    }

    {
        variant<int, float> const v( 3.14f );

        BOOST_TEST_THROWS( get<int>(v), bad_variant_access );
        BOOST_TEST_EQ( get_if<int>(&v), nullptr );

        BOOST_TEST_EQ( get<float>(v), 3.14f );
        BOOST_TEST_EQ( get_if<float>(&v), &get<float>(v) );

        BOOST_TEST_EQ( get<float>(std::move(v)), 3.14f );
    }

    {
        variant<int, float, float> v;

        BOOST_TEST_EQ( get<int>(v), 0 );
        BOOST_TEST_EQ( get_if<int>(&v), &get<int>(v) );

        BOOST_TEST_EQ( get<int>(std::move(v)), 0 );
    }

    {
        variant<int, float, float> const v;

        BOOST_TEST_EQ( get<int>(v), 0 );
        BOOST_TEST_EQ( get_if<int>(&v), &get<int>(v) );

        BOOST_TEST_EQ( get<int>(std::move(v)), 0 );
    }

    {
        variant<int, float, float> v( 1 );

        BOOST_TEST_EQ( get<int>(v), 1 );
        BOOST_TEST_EQ( get_if<int>(&v), &get<int>(v) );

        BOOST_TEST_EQ( get<int>(std::move(v)), 1 );
    }

    {
        variant<int, float, float> const v( 1 );

        BOOST_TEST_EQ( get<int>(v), 1 );
        BOOST_TEST_EQ( get_if<int>(&v), &get<int>(v) );

        BOOST_TEST_EQ( get<int>(std::move(v)), 1 );
    }

    {
        variant<int, int, float> v( 3.14f );

        BOOST_TEST_EQ( get<float>(v), 3.14f );
        BOOST_TEST_EQ( get_if<float>(&v), &get<float>(v) );

        BOOST_TEST_EQ( get<float>(std::move(v)), 3.14f );
    }

    {
        variant<int, int, float> const v( 3.14f );

        BOOST_TEST_EQ( get<float>(v), 3.14f );
        BOOST_TEST_EQ( get_if<float>(&v), &get<float>(v) );

        BOOST_TEST_EQ( get<float>(std::move(v)), 3.14f );
    }

    {
        variant<int> * p = 0;
        BOOST_TEST_EQ( get_if<int>(p), nullptr );
    }

    {
        variant<int const> * p = 0;
        BOOST_TEST_EQ( get_if<int const>(p), nullptr );
    }

    {
        variant<int> const * p = 0;
        BOOST_TEST_EQ( get_if<int>(p), nullptr );
    }

    {
        variant<int const> const * p = 0;
        BOOST_TEST_EQ( get_if<int const>(p), nullptr );
    }

    {
        variant<int, float> * p = 0;

        BOOST_TEST_EQ( get_if<int>(p), nullptr );
        BOOST_TEST_EQ( get_if<float>(p), nullptr );
    }

    {
        variant<int, float> const * p = 0;

        BOOST_TEST_EQ( get_if<int>(p), nullptr );
        BOOST_TEST_EQ( get_if<float>(p), nullptr );
    }

    {
        variant<int, int, float> * p = 0;
        BOOST_TEST_EQ( get_if<float>(p), nullptr );
    }

    {
        variant<int, int, float> const * p = 0;
        BOOST_TEST_EQ( get_if<float>(p), nullptr );
    }

    return boost::report_errors();
}
