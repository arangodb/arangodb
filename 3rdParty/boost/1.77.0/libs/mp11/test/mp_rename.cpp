
//  Copyright 2015-2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/core/lightweight_test_trait.hpp>
#include <boost/mp11/list.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

template<class... T> struct X {};
template<class... T> using Y = X<T...>;

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_rename;
    using boost::mp11::mp_apply;

    using L1 = mp_list<>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L1, mp_list>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L1, std::tuple>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L1, X>, X<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L1, Y>, Y<>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<mp_list, L1>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<std::tuple, L1>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<X, L1>, X<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<Y, L1>, Y<>>));

    using L2 = mp_list<char>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L2, mp_list>, mp_list<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L2, std::tuple>, std::tuple<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L2, X>, X<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L2, Y>, Y<char>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<mp_list, L2>, mp_list<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<std::tuple, L2>, std::tuple<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<X, L2>, X<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<Y, L2>, Y<char>>));

    using L3 = mp_list<char, double>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L3, mp_list>, mp_list<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L3, std::tuple>, std::tuple<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L3, X>, X<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L3, Y>, Y<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L3, std::pair>, std::pair<char, double>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<mp_list, L3>, mp_list<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<std::tuple, L3>, std::tuple<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<X, L3>, X<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<Y, L3>, Y<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<std::pair, L3>, std::pair<char, double>>));

    using L4 = mp_list<int, char, float>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L4, mp_list>, mp_list<int, char, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L4, std::tuple>, std::tuple<int, char, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L4, X>, X<int, char, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L4, Y>, Y<int, char, float>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<mp_list, L4>, mp_list<int, char, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<std::tuple, L4>, std::tuple<int, char, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<X, L4>, X<int, char, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<Y, L4>, Y<int, char, float>>));

    //

    using L5 = std::tuple<>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L5, mp_list>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L5, std::tuple>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L5, X>, X<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L5, Y>, Y<>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<mp_list, L5>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<std::tuple, L5>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<X, L5>, X<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<Y, L5>, Y<>>));

    using L6 = std::tuple<char>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L6, mp_list>, mp_list<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L6, std::tuple>, std::tuple<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L6, X>, X<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L6, Y>, Y<char>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<mp_list, L6>, mp_list<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<std::tuple, L6>, std::tuple<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<X, L6>, X<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<Y, L6>, Y<char>>));

    using L7 = std::tuple<char, double>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L7, mp_list>, mp_list<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L7, std::tuple>, std::tuple<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L7, X>, X<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L7, Y>, Y<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L7, std::pair>, std::pair<char, double>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<mp_list, L7>, mp_list<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<std::tuple, L7>, std::tuple<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<X, L7>, X<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<Y, L7>, Y<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<std::pair, L7>, std::pair<char, double>>));

    using L8 = std::tuple<int, char, float>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L8, mp_list>, mp_list<int, char, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L8, std::tuple>, std::tuple<int, char, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L8, X>, X<int, char, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L8, Y>, Y<int, char, float>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<mp_list, L8>, mp_list<int, char, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<std::tuple, L8>, std::tuple<int, char, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<X, L8>, X<int, char, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<Y, L8>, Y<int, char, float>>));

    //

    using L9 = std::pair<char, double>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L9, mp_list>, mp_list<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L9, std::tuple>, std::tuple<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L9, X>, X<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L9, Y>, Y<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rename<L9, std::pair>, std::pair<char, double>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<mp_list, L9>, mp_list<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<std::tuple, L9>, std::tuple<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<X, L9>, X<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<Y, L9>, Y<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply<std::pair, L9>, std::pair<char, double>>));

    //

    return boost::report_errors();
}
