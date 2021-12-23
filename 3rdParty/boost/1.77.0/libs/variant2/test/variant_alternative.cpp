
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

template<class I, class T> using var_alt_t = variant_alternative_t<I::value, T>;

int main()
{
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<void>>, void>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<void> const>, void const>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<void> volatile>, void volatile>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<void> const volatile>, void const volatile>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<char>&>, char&>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<char> const&>, char const&>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<char>&&>, char&&>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<char> const&&>, char const&&>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<void, int>>, void>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<void, int> const>, void const>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<void, int> volatile>, void volatile>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<void, int> const volatile>, void const volatile>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<char, int>&>, char&>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<char, int> const&>, char const&>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<char, int>&&>, char&&>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<char, int> const&&>, char const&&>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<1, variant<void, int>>, int>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<1, variant<void, int> const>, int const>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<1, variant<void, int> volatile>, int volatile>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<1, variant<void, int> const volatile>, int const volatile>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<1, variant<void, int>&>, int&>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<1, variant<void, int> const&>, int const&>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<1, variant<void, int>&&>, int&&>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<1, variant<void, int> const&&>, int const&&>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<void, int, float>>, void>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<void, int, float> const>, void const>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<void, int, float> volatile>, void volatile>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<void, int, float> const volatile>, void const volatile>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<char, int, float>&>, char&>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<char, int, float> const&>, char const&>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<char, int, float>&&>, char&&>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<0, variant<char, int, float> const&&>, char const&&>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<1, variant<void, int, float>>, int>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<1, variant<void, int, float> const>, int const>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<1, variant<void, int, float> volatile>, int volatile>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<1, variant<void, int, float> const volatile>, int const volatile>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<1, variant<void, int, float>&>, int&>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<1, variant<void, int, float> const&>, int const&>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<1, variant<void, int, float>&&>, int&&>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<1, variant<void, int, float> const&&>, int const&&>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<2, variant<void, int, float>>, float>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<2, variant<void, int, float> const>, float const>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<2, variant<void, int, float> volatile>, float volatile>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<2, variant<void, int, float> const volatile>, float const volatile>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<2, variant<void, int, float>&>, float&>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<2, variant<void, int, float> const&>, float const&>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<2, variant<void, int, float>&&>, float&&>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<variant_alternative_t<2, variant<void, int, float> const&&>, float const&&>));

    variant_alternative<0, void>();
    variant_alternative<0, void const>();
    variant_alternative<0, void volatile>();
    variant_alternative<0, void const volatile>();

    variant_alternative<0, int&>();
    variant_alternative<0, int const&>();
    variant_alternative<0, int&&>();
    variant_alternative<0, int const&&>();

    variant_alternative<0, variant<>>();
    variant_alternative<0, variant<> const>();
    variant_alternative<0, variant<> volatile>();
    variant_alternative<0, variant<> const volatile>();

    variant_alternative<0, variant<>&>();
    variant_alternative<0, variant<> const&>();
    variant_alternative<0, variant<>&&>();
    variant_alternative<0, variant<> const&&>();

    variant_alternative<1, variant<int>>();
    variant_alternative<1, variant<int> const>();
    variant_alternative<1, variant<int> volatile>();
    variant_alternative<1, variant<int> const volatile>();

    variant_alternative<1, variant<int>&>();
    variant_alternative<1, variant<int> const&>();
    variant_alternative<1, variant<int>&&>();
    variant_alternative<1, variant<int> const&&>();

    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<0>, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<0>, void const>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<0>, void volatile>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<0>, void const volatile>));

    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<0>, int&>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<0>, int const&>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<0>, int&&>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<0>, int const&&>));

    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<0>, variant<>>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<0>, variant<> const>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<0>, variant<> volatile>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<0>, variant<> const volatile>));

    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<0>, variant<>&>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<0>, variant<> const&>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<0>, variant<>&&>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<0>, variant<> const&&>));

    BOOST_TEST_TRAIT_TRUE((mp_valid<var_alt_t, mp_size_t<0>, variant<int>>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<var_alt_t, mp_size_t<0>, variant<int> const>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<var_alt_t, mp_size_t<0>, variant<int> volatile>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<var_alt_t, mp_size_t<0>, variant<int> const volatile>));

    BOOST_TEST_TRAIT_TRUE((mp_valid<var_alt_t, mp_size_t<0>, variant<int>&>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<var_alt_t, mp_size_t<0>, variant<int> const&>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<var_alt_t, mp_size_t<0>, variant<int>&&>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<var_alt_t, mp_size_t<0>, variant<int> const&&>));

    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<1>, variant<int>>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<1>, variant<int> const>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<1>, variant<int> volatile>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<1>, variant<int> const volatile>));

    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<1>, variant<int>&>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<1>, variant<int> const&>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<1>, variant<int>&&>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<var_alt_t, mp_size_t<1>, variant<int> const&&>));

    return boost::report_errors();
}
