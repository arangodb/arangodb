
// Copyright 2015-2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>

struct X1 {};
struct X2 {};
struct X3 {};
struct X4 {};

template<class T1, class T2> struct F {};

using boost::mp11::mp_push_back;
using boost::mp11::mp_push_front;

template<class T, class L> using rev_push_back = mp_push_back<L, T>;
template<class T, class L> using rev_push_front = mp_push_front<L, T>;

using boost::mp11::mp_plus;

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_reverse_fold;

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse_fold<mp_list<>, void, F>, void>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse_fold<mp_list<X1>, void, F>, F<X1, void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse_fold<mp_list<X1, X2>, void, F>, F<X1, F<X2, void>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse_fold<mp_list<X1, X2, X3>, void, F>, F<X1, F<X2, F<X3, void>>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse_fold<mp_list<X1, X2, X3, X4>, void, F>, F<X1, F<X2, F<X3, F<X4, void>>>>>));
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse_fold<std::tuple<>, void, F>, void>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse_fold<std::tuple<X1>, void, F>, F<X1, void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse_fold<std::tuple<X1, X2>, void, F>, F<X1, F<X2, void>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse_fold<std::tuple<X1, X2, X3>, void, F>, F<X1, F<X2, F<X3, void>>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse_fold<std::tuple<X1, X2, X3, X4>, void, F>, F<X1, F<X2, F<X3, F<X4, void>>>>>));
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse_fold<std::tuple<X1, X2, X3, X4>, mp_list<>, rev_push_back>, mp_list<X4, X3, X2, X1>>));
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse_fold<std::tuple<X1, X2, X3, X4>, mp_list<>, rev_push_front>, mp_list<X1, X2, X3, X4>>));
    }

    using boost::mp11::mp_iota_c;
    using boost::mp11::mp_reverse;
    using boost::mp11::mp_size_t;

    {
        int const N = 37;

        using L = mp_iota_c<N>;

        using R1 = mp_reverse_fold<L, mp_list<>, rev_push_front>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R1, L>));

        using R2 = mp_reverse_fold<L, mp_list<>, rev_push_back>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R2, mp_reverse<L>>));

        using R3 = mp_reverse_fold<L, mp_size_t<0>, mp_plus>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R3, mp_size_t<N*(N-1)/2>>));
    }

    return boost::report_errors();
}
