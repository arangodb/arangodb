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
struct X5 {};

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_intersperse;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_intersperse<mp_list<>, void>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_intersperse<mp_list<X1>, void>, mp_list<X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_intersperse<mp_list<X1, X2>, void>, mp_list<X1, void, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_intersperse<mp_list<X1, X2, X3>, void>, mp_list<X1, void, X2, void, X3>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_intersperse<mp_list<X1, X2, X3, X4>, void>, mp_list<X1, void, X2, void, X3, void, X4>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_intersperse<mp_list<X1, X2, X3, X4, X5>, void>, mp_list<X1, void, X2, void, X3, void, X4, void, X5>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_intersperse<std::tuple<>, void>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_intersperse<std::tuple<X1>, void>, std::tuple<X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_intersperse<std::tuple<X1, X2>, void>, std::tuple<X1, void, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_intersperse<std::tuple<X1, X2, X3>, void>, std::tuple<X1, void, X2, void, X3>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_intersperse<std::tuple<X1, X2, X3, X4>, void>, std::tuple<X1, void, X2, void, X3, void, X4>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_intersperse<std::tuple<X1, X2, X3, X4, X5>, void>, std::tuple<X1, void, X2, void, X3, void, X4, void, X5>>));

    return boost::report_errors();
}
