
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>
#include <boost/mp11.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <cstddef>

using namespace boost::variant2;
using namespace boost::mp11;

template<class T> using var_size_t = mp_size_t<variant_size<T>::value>;

int main()
{
    BOOST_TEST_EQ( (variant_size<variant<>>::value), 0 );
    BOOST_TEST_EQ( (variant_size<variant<> const>::value), 0 );
    BOOST_TEST_EQ( (variant_size<variant<> volatile>::value), 0 );
    BOOST_TEST_EQ( (variant_size<variant<> const volatile>::value), 0 );

    BOOST_TEST_EQ( (variant_size<variant<>&>::value), 0 );
    BOOST_TEST_EQ( (variant_size<variant<> const&>::value), 0 );
    BOOST_TEST_EQ( (variant_size<variant<>&&>::value), 0 );
    BOOST_TEST_EQ( (variant_size<variant<> const&&>::value), 0 );

    BOOST_TEST_EQ( (variant_size<variant<void>>::value), 1 );
    BOOST_TEST_EQ( (variant_size<variant<void> const>::value), 1 );
    BOOST_TEST_EQ( (variant_size<variant<void> volatile>::value), 1 );
    BOOST_TEST_EQ( (variant_size<variant<void> const volatile>::value), 1 );

    BOOST_TEST_EQ( (variant_size<variant<void>&>::value), 1 );
    BOOST_TEST_EQ( (variant_size<variant<void> const&>::value), 1 );
    BOOST_TEST_EQ( (variant_size<variant<void>&&>::value), 1 );
    BOOST_TEST_EQ( (variant_size<variant<void> const&&>::value), 1 );

    BOOST_TEST_EQ( (variant_size<variant<void, void>>::value), 2 );
    BOOST_TEST_EQ( (variant_size<variant<void, void> const>::value), 2 );
    BOOST_TEST_EQ( (variant_size<variant<void, void> volatile>::value), 2 );
    BOOST_TEST_EQ( (variant_size<variant<void, void> const volatile>::value), 2 );

    BOOST_TEST_EQ( (variant_size<variant<void, void>&>::value), 2 );
    BOOST_TEST_EQ( (variant_size<variant<void, void> const&>::value), 2 );
    BOOST_TEST_EQ( (variant_size<variant<void, void>&&>::value), 2 );
    BOOST_TEST_EQ( (variant_size<variant<void, void> const&&>::value), 2 );

    BOOST_TEST_EQ( (variant_size<variant<void, void, void>>::value), 3 );
    BOOST_TEST_EQ( (variant_size<variant<void, void, void> const>::value), 3 );
    BOOST_TEST_EQ( (variant_size<variant<void, void, void> volatile>::value), 3 );
    BOOST_TEST_EQ( (variant_size<variant<void, void, void> const volatile>::value), 3 );

    BOOST_TEST_EQ( (variant_size<variant<void, void, void>&>::value), 3 );
    BOOST_TEST_EQ( (variant_size<variant<void, void, void> const&>::value), 3 );
    BOOST_TEST_EQ( (variant_size<variant<void, void, void>&&>::value), 3 );
    BOOST_TEST_EQ( (variant_size<variant<void, void, void> const&&>::value), 3 );

    BOOST_TEST_EQ( (variant_size<variant<void, void, void, void>>::value), 4 );
    BOOST_TEST_EQ( (variant_size<variant<void, void, void, void> const>::value), 4 );
    BOOST_TEST_EQ( (variant_size<variant<void, void, void, void> volatile>::value), 4 );
    BOOST_TEST_EQ( (variant_size<variant<void, void, void, void> const volatile>::value), 4 );

    BOOST_TEST_EQ( (variant_size<variant<void, void, void, void>&>::value), 4 );
    BOOST_TEST_EQ( (variant_size<variant<void, void, void, void> const&>::value), 4 );
    BOOST_TEST_EQ( (variant_size<variant<void, void, void, void>&&>::value), 4 );
    BOOST_TEST_EQ( (variant_size<variant<void, void, void, void> const&&>::value), 4 );

    variant_size<void>();
    variant_size<void const>();
    variant_size<void volatile>();
    variant_size<void const volatile>();

    variant_size<int&>();
    variant_size<int const&>();
    variant_size<int&&>();
    variant_size<int const&&>();

    BOOST_TEST_TRAIT_FALSE((mp_valid<var_size_t, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_size_t, void const>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_size_t, void volatile>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_size_t, void const volatile>));

    BOOST_TEST_TRAIT_FALSE((mp_valid<var_size_t, int&>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_size_t, int const&>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_size_t, int&&>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_size_t, int const&&>));

    BOOST_TEST_TRAIT_TRUE((mp_valid<var_size_t, variant<>>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<var_size_t, variant<> const>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<var_size_t, variant<> volatile>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<var_size_t, variant<> const volatile>));

    BOOST_TEST_TRAIT_TRUE((mp_valid<var_size_t, variant<>&>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<var_size_t, variant<> const&>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<var_size_t, variant<>&&>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<var_size_t, variant<> const&&>));

    return boost::report_errors();
}
