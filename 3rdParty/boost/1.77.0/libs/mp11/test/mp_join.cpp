// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

struct X1 {};
struct X2 {};
struct X3 {};
struct X4 {};

struct S {};

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_join;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_join<mp_list<mp_list<>>, S>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_join<mp_list<mp_list<X1>>, S>, mp_list<X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_join<mp_list<mp_list<X1, X2>>, S>, mp_list<X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_join<mp_list<mp_list<X1, X2, X3>>, S>, mp_list<X1, X2, X3>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_join<mp_list<mp_list<>, mp_list<>>, S>, mp_list<S>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_join<mp_list<mp_list<>, mp_list<>, mp_list<>>, S>, mp_list<S, S>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_join<mp_list<mp_list<>, mp_list<>, mp_list<>, mp_list<>>, S>, mp_list<S, S, S>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_join<mp_list<mp_list<X1>, mp_list<X2>>, S>, mp_list<X1, S, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_join<mp_list<mp_list<X1, X2>, mp_list<X3, X4>>, S>, mp_list<X1, X2, S, X3, X4>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_join<mp_list<mp_list<>, mp_list<X1, X2>, mp_list<>, mp_list<X3, X4>, mp_list<>>, S>, mp_list<S, X1, X2, S, S, X3, X4, S>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_join<std::tuple<std::tuple<>>, S>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_join<std::tuple<std::tuple<X1>>, S>, std::tuple<X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_join<std::tuple<std::tuple<X1, X2>>, S>, std::tuple<X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_join<std::tuple<std::tuple<X1, X2, X3>>, S>, std::tuple<X1, X2, X3>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_join<std::tuple<std::tuple<>, std::tuple<>>, S>, std::tuple<S>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_join<std::tuple<std::tuple<>, std::tuple<>, std::tuple<>>, S>, std::tuple<S, S>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_join<std::tuple<std::tuple<>, std::tuple<>, std::tuple<>, std::tuple<>>, S>, std::tuple<S, S, S>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_join<std::tuple<std::tuple<X1>, std::tuple<X2>>, S>, std::tuple<X1, S, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_join<std::tuple<std::tuple<X1, X2>, std::tuple<X3, X4>>, S>, std::tuple<X1, X2, S, X3, X4>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_join<std::tuple<std::tuple<>, std::tuple<X1, X2>, std::tuple<>, std::tuple<X3, X4>, std::tuple<>>, S>, std::tuple<S, X1, X2, S, S, X3, X4, S>>));

    return boost::report_errors();
}
