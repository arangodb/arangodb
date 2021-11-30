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
    using boost::mp11::mp_split;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_split<mp_list<>, void>, mp_list<mp_list<>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_split<mp_list<X1>, void>, mp_list<mp_list<X1>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_split<mp_list<X1, X2>, void>, mp_list<mp_list<X1, X2>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_split<mp_list<X1, X2, X3>, void>, mp_list<mp_list<X1, X2, X3>>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_split<mp_list<S>, S>, mp_list<mp_list<>, mp_list<>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_split<mp_list<S, S>, S>, mp_list<mp_list<>, mp_list<>, mp_list<>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_split<mp_list<S, S, S>, S>, mp_list<mp_list<>, mp_list<>, mp_list<>, mp_list<>>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_split<mp_list<X1, S, X2>, S>, mp_list<mp_list<X1>, mp_list<X2>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_split<mp_list<X1, X2, S, X3, X4>, S>, mp_list<mp_list<X1, X2>, mp_list<X3, X4>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_split<mp_list<S, X1, X2, S, S, X3, X4, S>, S>, mp_list<mp_list<>, mp_list<X1, X2>, mp_list<>, mp_list<X3, X4>, mp_list<>>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_split<std::tuple<>, void>, std::tuple<std::tuple<>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_split<std::tuple<X1>, void>, std::tuple<std::tuple<X1>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_split<std::tuple<X1, X2>, void>, std::tuple<std::tuple<X1, X2>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_split<std::tuple<X1, X2, X3>, void>, std::tuple<std::tuple<X1, X2, X3>>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_split<std::tuple<S>, S>, std::tuple<std::tuple<>, std::tuple<>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_split<std::tuple<S, S>, S>, std::tuple<std::tuple<>, std::tuple<>, std::tuple<>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_split<std::tuple<S, S, S>, S>, std::tuple<std::tuple<>, std::tuple<>, std::tuple<>, std::tuple<>>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_split<std::tuple<X1, S, X2>, S>, std::tuple<std::tuple<X1>, std::tuple<X2>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_split<std::tuple<X1, X2, S, X3, X4>, S>, std::tuple<std::tuple<X1, X2>, std::tuple<X3, X4>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_split<std::tuple<S, X1, X2, S, S, X3, X4, S>, S>, std::tuple<std::tuple<>, std::tuple<X1, X2>, std::tuple<>, std::tuple<X3, X4>, std::tuple<>>>));

    return boost::report_errors();
}
