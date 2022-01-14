// Copyright 2020 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>

struct X1 {};
struct X2 {};
struct X3 {};

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_power_set;

    BOOST_TEST_TRAIT_TRUE((std::is_same< mp_power_set< mp_list<> >, mp_list<mp_list<>> >));
    BOOST_TEST_TRAIT_TRUE((std::is_same< mp_power_set< mp_list<X1> >, mp_list< mp_list<>, mp_list<X1> > >));
    BOOST_TEST_TRAIT_TRUE((std::is_same< mp_power_set< mp_list<X2, X1> >, mp_list< mp_list<>, mp_list<X1>, mp_list<X2>, mp_list<X2, X1> > >));
    BOOST_TEST_TRAIT_TRUE((std::is_same< mp_power_set< mp_list<X1, X1> >, mp_list< mp_list<>, mp_list<X1>, mp_list<X1>, mp_list<X1, X1> > >));
    BOOST_TEST_TRAIT_TRUE((std::is_same< mp_power_set< mp_list<X3, X2, X1> >, mp_list< mp_list<>, mp_list<X1>, mp_list<X2>, mp_list<X2, X1>, mp_list<X3>, mp_list<X3, X1>, mp_list<X3, X2>, mp_list<X3, X2, X1> > >));

    BOOST_TEST_TRAIT_TRUE((std::is_same< mp_power_set< std::tuple<> >, std::tuple<std::tuple<>> >));
    BOOST_TEST_TRAIT_TRUE((std::is_same< mp_power_set< std::tuple<X1> >, std::tuple< std::tuple<>, std::tuple<X1> > >));
    BOOST_TEST_TRAIT_TRUE((std::is_same< mp_power_set< std::tuple<X2, X1> >, std::tuple< std::tuple<>, std::tuple<X1>, std::tuple<X2>, std::tuple<X2, X1> > >));
    BOOST_TEST_TRAIT_TRUE((std::is_same< mp_power_set< std::tuple<X1, X1> >, std::tuple< std::tuple<>, std::tuple<X1>, std::tuple<X1>, std::tuple<X1, X1> > >));
    BOOST_TEST_TRAIT_TRUE((std::is_same< mp_power_set< std::tuple<X3, X2, X1> >, std::tuple< std::tuple<>, std::tuple<X1>, std::tuple<X2>, std::tuple<X2, X1>, std::tuple<X3>, std::tuple<X3, X1>, std::tuple<X3, X2>, std::tuple<X3, X2, X1> > >));

    return boost::report_errors();
}
