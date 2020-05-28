
// Copyright 2015, 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>

struct X1 {};
struct X2 {};
struct X3 {};
struct X4 {};

template<class T1, class T2> struct F {};

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_fold_q;
    using boost::mp11::mp_quote;

    using Q = mp_quote<F>;

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_fold_q<mp_list<>, void, Q>, void>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_fold_q<mp_list<X1>, void, Q>, F<void, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_fold_q<mp_list<X1, X2>, void, Q>, F<F<void, X1>, X2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_fold_q<mp_list<X1, X2, X3>, void, Q>, F<F<F<void, X1>, X2>, X3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_fold_q<mp_list<X1, X2, X3, X4>, void, Q>, F<F<F<F<void, X1>, X2>, X3>, X4>>));
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_fold_q<std::tuple<>, void, Q>, void>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_fold_q<std::tuple<X1>, void, Q>, F<void, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_fold_q<std::tuple<X1, X2>, void, Q>, F<F<void, X1>, X2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_fold_q<std::tuple<X1, X2, X3>, void, Q>, F<F<F<void, X1>, X2>, X3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_fold_q<std::tuple<X1, X2, X3, X4>, void, Q>, F<F<F<F<void, X1>, X2>, X3>, X4>>));
    }

    using boost::mp11::mp_push_back;

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_fold_q<std::tuple<X1, X2, X3, X4>, mp_list<>, mp_quote<mp_push_back>>, mp_list<X1, X2, X3, X4>>));
    }

    using boost::mp11::mp_push_front;

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_fold_q<std::tuple<X1, X2, X3, X4>, mp_list<>, mp_quote<mp_push_front>>, mp_list<X4, X3, X2, X1>>));
    }

    return boost::report_errors();
}
