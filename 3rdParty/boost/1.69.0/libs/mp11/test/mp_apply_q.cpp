
//  Copyright 2015-2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/core/lightweight_test_trait.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/utility.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

template<class... T> struct X {};
template<class... T> using Y = X<T...>;

struct Q
{
    template<class... T> using fn = X<T...>;
};

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_quote;
    using boost::mp11::mp_apply_q;

    using L1 = mp_list<>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<mp_list>, L1>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<std::tuple>, L1>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<X>, L1>, X<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<Y>, L1>, Y<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<Q, L1>, X<>>));

    using L2 = mp_list<char>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<mp_list>, L2>, mp_list<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<std::tuple>, L2>, std::tuple<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<X>, L2>, X<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<Y>, L2>, Y<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<Q, L2>, X<char>>));

    using L3 = mp_list<char, double>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<mp_list>, L3>, mp_list<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<std::tuple>, L3>, std::tuple<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<X>, L3>, X<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<Y>, L3>, Y<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<std::pair>, L3>, std::pair<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<Q, L3>, X<char, double>>));

    using L4 = mp_list<int, char, float>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<mp_list>, L4>, mp_list<int, char, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<std::tuple>, L4>, std::tuple<int, char, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<X>, L4>, X<int, char, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<Y>, L4>, Y<int, char, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<Q, L4>, X<int, char, float>>));

    //

    using L5 = std::tuple<>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<mp_list>, L5>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<std::tuple>, L5>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<X>, L5>, X<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<Y>, L5>, Y<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<Q, L5>, X<>>));

    using L6 = std::tuple<char>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<mp_list>, L6>, mp_list<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<std::tuple>, L6>, std::tuple<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<X>, L6>, X<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<Y>, L6>, Y<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<Q, L6>, X<char>>));

    using L7 = std::tuple<char, double>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<mp_list>, L7>, mp_list<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<std::tuple>, L7>, std::tuple<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<X>, L7>, X<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<Y>, L7>, Y<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<std::pair>, L7>, std::pair<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<Q, L7>, X<char, double>>));

    using L8 = std::tuple<int, char, float>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<mp_list>, L8>, mp_list<int, char, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<std::tuple>, L8>, std::tuple<int, char, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<X>, L8>, X<int, char, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<Y>, L8>, Y<int, char, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<Q, L8>, X<int, char, float>>));

    //

    using L9 = std::pair<char, double>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<mp_list>, L9>, mp_list<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<std::tuple>, L9>, std::tuple<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<X>, L9>, X<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<Y>, L9>, Y<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<mp_quote<std::pair>, L9>, std::pair<char, double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_apply_q<Q, L9>, X<char, double>>));

    //

    return boost::report_errors();
}
