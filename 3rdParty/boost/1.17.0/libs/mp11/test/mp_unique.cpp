
//  Copyright 2015 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_unique;

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique<mp_list<>>, mp_list<>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique<mp_list<void>>, mp_list<void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique<mp_list<void, void>>, mp_list<void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique<mp_list<void, void, void>>, mp_list<void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique<mp_list<void, void, void, void>>, mp_list<void>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique<mp_list<void, int>>, mp_list<void, int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique<mp_list<void, void, void, int, int, int>>, mp_list<void, int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique<mp_list<void, int, void, int, int, void, int, int, int>>, mp_list<void, int>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique<mp_list<void, int, char[]>>, mp_list<void, int, char[]>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique<mp_list<void, int, char[], void, int, char[], void, int, char[]>>, mp_list<void, int, char[]>>));
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique<std::tuple<>>, std::tuple<>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique<std::tuple<void>>, std::tuple<void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique<std::tuple<void, void>>, std::tuple<void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique<std::tuple<void, void, void>>, std::tuple<void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique<std::tuple<void, void, void, void>>, std::tuple<void>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique<std::tuple<void, int>>, std::tuple<void, int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique<std::tuple<void, void, void, int, int, int>>, std::tuple<void, int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique<std::tuple<void, int, void, int, int, void, int, int, int>>, std::tuple<void, int>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique<std::tuple<void, int, char[]>>, std::tuple<void, int, char[]>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique<std::tuple<void, int, char[], void, int, char[], void, int, char[]>>, std::tuple<void, int, char[]>>));
    }

    return boost::report_errors();
}
