
//  Copyright 2015 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

struct X1 {};
struct X2 {};
struct X3 {};

struct Y1 {};

struct Z1 {};
struct Z2 {};

template<class T1, class T2, class T3> struct F {};

template<class T> struct F1 {};

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_product;
    using boost::mp11::mp_product_q;
    using boost::mp11::mp_quote;

    {
        using L1 = std::tuple<X1, X2, X3>;
        using L2 = mp_list<Y1>;
        using L3 = std::pair<Z1, Z2>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_product<F, L1, L2, L3>, std::tuple<F<X1, Y1, Z1>, F<X1, Y1, Z2>, F<X2, Y1, Z1>, F<X2, Y1, Z2>, F<X3, Y1, Z1>, F<X3, Y1, Z2>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_product_q<mp_quote<F>, L1, L2, L3>, std::tuple<F<X1, Y1, Z1>, F<X1, Y1, Z2>, F<X2, Y1, Z1>, F<X2, Y1, Z2>, F<X3, Y1, Z1>, F<X3, Y1, Z2>>>));
    }

    {
        using L1 = std::tuple<X1, X2, X3>;
        using L2 = mp_list<>;
        using L3 = std::pair<Z1, Z2>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_product<F, L1, L2, L3>, std::tuple<>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_product_q<mp_quote<F>, L1, L2, L3>, std::tuple<>>));
    }

    {
        using L1 = std::tuple<X1, X2, X3>;
        using L2 = mp_list<X1, X2, X3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_product<F1, L1>, std::tuple<F1<X1>, F1<X2>, F1<X3>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_product_q<mp_quote<F1>, L1>, std::tuple<F1<X1>, F1<X2>, F1<X3>>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_product<F1, L2>, mp_list<F1<X1>, F1<X2>, F1<X3>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_product_q<mp_quote<F1>, L2>, mp_list<F1<X1>, F1<X2>, F1<X3>>>));
    }

    return boost::report_errors();
}
