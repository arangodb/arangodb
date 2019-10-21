
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/set.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_is_set;
    using boost::mp11::mp_true;
    using boost::mp11::mp_false;

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<void>, mp_false>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<int>, mp_false>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<char[]>, mp_false>));
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<mp_list<>>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<mp_list<void>>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<mp_list<void, void>>, mp_false>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<mp_list<void, void, void>>, mp_false>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<mp_list<void, void, void, void>>, mp_false>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<mp_list<void, int>>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<mp_list<void, int, void>>, mp_false>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<mp_list<void, int, int>>, mp_false>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<mp_list<void, int, char[]>>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<mp_list<void, int, char[], void>>, mp_false>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<mp_list<void, int, char[], int>>, mp_false>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<mp_list<void, int, char[], char[]>>, mp_false>));
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<std::tuple<>>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<std::tuple<void>>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<std::tuple<void, void>>, mp_false>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<std::tuple<void, void, void>>, mp_false>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<std::tuple<void, void, void, void>>, mp_false>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<std::tuple<void, int>>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<std::tuple<void, int, void>>, mp_false>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<std::tuple<void, int, int>>, mp_false>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<std::tuple<void, int, char[]>>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<std::tuple<void, int, char[], void>>, mp_false>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<std::tuple<void, int, char[], int>>, mp_false>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<std::tuple<void, int, char[], char[]>>, mp_false>));
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<std::pair<void, void>>, mp_false>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<std::pair<void, int>>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<std::pair<int, void>>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_set<std::pair<int, int>>, mp_false>));
    }

    return boost::report_errors();
}
