
// Copyright 2015 Peter Dimov.
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

struct X1 {};
struct X2 {};
struct X3 {};
struct X4 {};
struct X5 {};
struct X6 {};

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_append;

    using L1 = mp_list<char[1], char[1]>;
    using L2 = mp_list<char[2], char[2]>;
    using L3 = mp_list<char[3], char[3]>;
    using L4 = mp_list<char[4], char[4]>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<L1>, mp_list<char[1], char[1]>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<L1, L2>, mp_list<char[1], char[1], char[2], char[2]>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<L1, L2, L3>, mp_list<char[1], char[1], char[2], char[2], char[3], char[3]>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<L1, L2, L3, L4>, mp_list<char[1], char[1], char[2], char[2], char[3], char[3], char[4], char[4]>>));

    //

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::tuple<>>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::tuple<>, std::tuple<X1>>, std::tuple<X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::tuple<>, std::tuple<X1>, std::tuple<X2>>, std::tuple<X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::tuple<>, std::tuple<X1>, std::tuple<X2>, std::tuple<X3>>, std::tuple<X1, X2, X3>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::tuple<>, std::tuple<X1>, std::tuple<X2>, std::tuple<X3>, std::tuple<X4>>, std::tuple<X1, X2, X3, X4>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::tuple<>, std::tuple<X1>, std::tuple<X2>, std::tuple<X3>, std::tuple<X4>, std::tuple<X5>>, std::tuple<X1, X2, X3, X4, X5>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::tuple<>, std::tuple<X1>, std::tuple<X2>, std::tuple<X3>, std::tuple<X4>, std::tuple<X5>, std::tuple<X6>>, std::tuple<X1, X2, X3, X4, X5, X6>>));

    //

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::tuple<>, mp_list<>>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::tuple<>, mp_list<X1>>, std::tuple<X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::tuple<>, mp_list<X1>, std::tuple<X2>>, std::tuple<X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::tuple<>, mp_list<X1>, std::tuple<X2>, mp_list<X3>>, std::tuple<X1, X2, X3>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::tuple<>, mp_list<X1>, std::tuple<X2>, mp_list<X3>, std::tuple<X4>>, std::tuple<X1, X2, X3, X4>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::tuple<>, mp_list<X1>, std::tuple<X2>, mp_list<X3>, std::tuple<X4>, mp_list<X5>>, std::tuple<X1, X2, X3, X4, X5>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::tuple<>, mp_list<X1>, std::tuple<X2>, mp_list<X3>, std::tuple<X4>, mp_list<X5>, std::tuple<X6>>, std::tuple<X1, X2, X3, X4, X5, X6>>));

    //

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::tuple<>, std::pair<X1, X2>>, std::tuple<X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::tuple<>, std::pair<X1, X2>, std::pair<X3, X4>>, std::tuple<X1, X2, X3, X4>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::tuple<>, std::pair<X1, X2>, std::pair<X3, X4>, std::pair<X5, X6>>, std::tuple<X1, X2, X3, X4, X5, X6>>));

    //

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::pair<X1, X2>>, std::pair<X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::pair<X1, X2>, mp_list<>>, std::pair<X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::pair<X1, X2>, mp_list<>, mp_list<>>, std::pair<X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::pair<X1, X2>, mp_list<>, mp_list<>, mp_list<>>, std::pair<X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_append<std::pair<X1, X2>, mp_list<>, mp_list<>, mp_list<>, mp_list<>>, std::pair<X1, X2>>));

    //

    return boost::report_errors();
}
