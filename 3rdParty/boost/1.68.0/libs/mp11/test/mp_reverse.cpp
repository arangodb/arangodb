
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
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <type_traits>
#include <tuple>

struct X1 {};
struct X2 {};
struct X3 {};
struct X4 {};
struct X5 {};
struct X6 {};
struct X7 {};
struct X8 {};
struct X9 {};
struct X10 {};
struct X11 {};
struct X12 {};

using boost::mp11::mp_plus;

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_reverse;

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<mp_list<>>, mp_list<>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<mp_list<X1>>, mp_list<X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<mp_list<X1, X2>>, mp_list<X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<mp_list<X1, X2, X3>>, mp_list<X3, X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<mp_list<X1, X2, X3, X4>>, mp_list<X4, X3, X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<mp_list<X1, X2, X3, X4, X5>>, mp_list<X5, X4, X3, X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<mp_list<X1, X2, X3, X4, X5, X6>>, mp_list<X6, X5, X4, X3, X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<mp_list<X1, X2, X3, X4, X5, X6, X7>>, mp_list<X7, X6, X5, X4, X3, X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<mp_list<X1, X2, X3, X4, X5, X6, X7, X8>>, mp_list<X8, X7, X6, X5, X4, X3, X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<mp_list<X1, X2, X3, X4, X5, X6, X7, X8, X9>>, mp_list<X9, X8, X7, X6, X5, X4, X3, X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<mp_list<X1, X2, X3, X4, X5, X6, X7, X8, X9, X10>>, mp_list<X10, X9, X8, X7, X6, X5, X4, X3, X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<mp_list<X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11>>, mp_list<X11, X10, X9, X8, X7, X6, X5, X4, X3, X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<mp_list<X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12>>, mp_list<X12, X11, X10, X9, X8, X7, X6, X5, X4, X3, X2, X1>>));
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<std::tuple<>>, std::tuple<>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<std::tuple<X1>>, std::tuple<X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<std::tuple<X1, X2>>, std::tuple<X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<std::tuple<X1, X2, X3>>, std::tuple<X3, X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<std::tuple<X1, X2, X3, X4>>, std::tuple<X4, X3, X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<std::tuple<X1, X2, X3, X4, X5>>, std::tuple<X5, X4, X3, X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<std::tuple<X1, X2, X3, X4, X5, X6>>, std::tuple<X6, X5, X4, X3, X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<std::tuple<X1, X2, X3, X4, X5, X6, X7>>, std::tuple<X7, X6, X5, X4, X3, X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<std::tuple<X1, X2, X3, X4, X5, X6, X7, X8>>, std::tuple<X8, X7, X6, X5, X4, X3, X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<std::tuple<X1, X2, X3, X4, X5, X6, X7, X8, X9>>, std::tuple<X9, X8, X7, X6, X5, X4, X3, X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<std::tuple<X1, X2, X3, X4, X5, X6, X7, X8, X9, X10>>, std::tuple<X10, X9, X8, X7, X6, X5, X4, X3, X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<std::tuple<X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11>>, std::tuple<X11, X10, X9, X8, X7, X6, X5, X4, X3, X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<std::tuple<X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12>>, std::tuple<X12, X11, X10, X9, X8, X7, X6, X5, X4, X3, X2, X1>>));
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_reverse<std::pair<X1, X2>>, std::pair<X2, X1>>));
    }

    using boost::mp11::mp_iota_c;
    using boost::mp11::mp_size_t;
    using boost::mp11::mp_transform;
    using boost::mp11::mp_fill;

    {
        int const N = 37;

        using L = mp_iota_c<N>;

        using R1 = mp_reverse<L>;
        using R2 = mp_reverse<R1>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<R2, L>));

        using R3 = mp_transform<mp_plus, R1, R2>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<R3, mp_fill<L, mp_size_t<N-1>>>));
    }

    return boost::report_errors();
}
