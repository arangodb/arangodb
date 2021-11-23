
// Copyright 2015, 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/list.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

struct X1 {};
struct X2 {};
struct X3 {};

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_assign;

    using L1 = mp_list<int, void(), float[]>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_assign<L1, mp_list<>>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_assign<L1, mp_list<X1>>, mp_list<X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_assign<L1, mp_list<X1, X2>>, mp_list<X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_assign<L1, mp_list<X1, X2, X3>>, mp_list<X1, X2, X3>>));

    //

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_assign<L1, std::tuple<>>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_assign<L1, std::tuple<X1>>, mp_list<X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_assign<L1, std::tuple<X1, X2>>, mp_list<X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_assign<L1, std::tuple<X1, X2, X3>>, mp_list<X1, X2, X3>>));

    //

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_assign<L1, std::pair<X1, X2>>, mp_list<X1, X2>>));

    //

    using L2 = std::tuple<int, char, float>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_assign<L2, mp_list<>>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_assign<L2, mp_list<X1>>, std::tuple<X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_assign<L2, mp_list<X1, X2>>, std::tuple<X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_assign<L2, mp_list<X1, X2, X3>>, std::tuple<X1, X2, X3>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_assign<L2, std::pair<X1, X2>>, std::tuple<X1, X2>>));

    //

    using L3 = std::pair<int, char>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_assign<L3, mp_list<X1, X2>>, std::pair<X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_assign<L3, std::pair<X1, X2>>, std::pair<X1, X2>>));

    //

    return boost::report_errors();
}
