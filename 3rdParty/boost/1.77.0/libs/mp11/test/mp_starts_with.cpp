// Copyright 2018 Glen Joseph Fernandes
// (glenjofe@gmail.com)
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
#include <utility>

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_starts_with;
    using boost::mp11::mp_true;
    using boost::mp11::mp_false;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_starts_with<mp_list<>,
        mp_list<> >, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_starts_with<mp_list<>,
        mp_list<int> >, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_starts_with<mp_list<int>,
        mp_list<> >, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_starts_with<mp_list<>,
        std::tuple<int> >, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_starts_with<std::tuple<int>,
        mp_list<> >, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_starts_with<mp_list<int>,
        mp_list<int> >, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_starts_with<mp_list<int>,
        mp_list<char> >, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_starts_with<std::tuple<int>,
        mp_list<int> >, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_starts_with<mp_list<int>,
        std::tuple<char> >, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_starts_with<mp_list<int, void>,
        mp_list<int> >, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_starts_with<mp_list<int, void>,
        mp_list<char> >, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_starts_with<mp_list<int>,
        mp_list<int, void> >, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_starts_with<std::pair<int, char>,
        mp_list<int> >, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_starts_with<mp_list<int>,
        std::pair<int, char> >, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_starts_with<mp_list<int, void>,
        mp_list<int, void> >, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_starts_with<mp_list<int, void>,
        mp_list<int, int> >, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_starts_with<std::tuple<int, char>,
        mp_list<int, char> >, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_starts_with<mp_list<int, char>,
        std::tuple<int, int> >, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_starts_with<mp_list<int, void, int>,
        mp_list<int, void> >, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_starts_with<mp_list<int, void, int>,
        mp_list<int, int> >, mp_false>));
    return boost::report_errors();
}
