// Copyright 2020 Peter Dimov
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

template<class T, class U> using is_same = typename std::is_same<T, U>::type;

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_pairwise_fold;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pairwise_fold<mp_list<>, mp_list>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pairwise_fold<mp_list<X1>, mp_list>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pairwise_fold<mp_list<X1, X2>, mp_list>, mp_list<mp_list<X1, X2>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pairwise_fold<mp_list<X1, X2, X3>, mp_list>, mp_list<mp_list<X1, X2>, mp_list<X2, X3>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pairwise_fold<mp_list<X1, X2, X3, X4>, mp_list>, mp_list<mp_list<X1, X2>, mp_list<X2, X3>, mp_list<X3, X4>>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pairwise_fold<mp_list<>, std::pair>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pairwise_fold<mp_list<X1>, std::pair>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pairwise_fold<mp_list<X1, X2>, std::pair>, mp_list<std::pair<X1, X2>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pairwise_fold<mp_list<X1, X2, X3>, std::pair>, mp_list<std::pair<X1, X2>, std::pair<X2, X3>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pairwise_fold<mp_list<X1, X2, X3, X4>, std::pair>, mp_list<std::pair<X1, X2>, std::pair<X2, X3>, std::pair<X3, X4>>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pairwise_fold<std::tuple<>, std::pair>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pairwise_fold<std::tuple<X1>, std::pair>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pairwise_fold<std::tuple<X1, X2>, std::pair>, std::tuple<std::pair<X1, X2>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pairwise_fold<std::tuple<X1, X2, X3>, std::pair>, std::tuple<std::pair<X1, X2>, std::pair<X2, X3>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pairwise_fold<std::tuple<X1, X2, X3, X4>, std::pair>, std::tuple<std::pair<X1, X2>, std::pair<X2, X3>, std::pair<X3, X4>>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pairwise_fold<std::tuple<>, is_same>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pairwise_fold<std::tuple<X1>, is_same>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pairwise_fold<std::tuple<X1, X2>, is_same>, std::tuple<is_same<X1, X2>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pairwise_fold<std::tuple<X1, X2, X2>, is_same>, std::tuple<is_same<X1, X2>, is_same<X2, X2>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pairwise_fold<std::tuple<X1, X2, X2, X1>, is_same>, std::tuple<is_same<X1, X2>, is_same<X2, X2>, is_same<X2, X1>>>));

    return boost::report_errors();
}
