
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/map.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

int main()
{
    using boost::mp11::mp_is_map;
    using boost::mp11::mp_list;
    using boost::mp11::mp_true;
    using boost::mp11::mp_false;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<void>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<mp_list<>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<std::tuple<>>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<mp_list<void>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<std::tuple<void>>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<mp_list<mp_list<>>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<std::tuple<std::tuple<>>>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<mp_list<mp_list<int>>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<std::tuple<std::tuple<int>>>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<mp_list<std::pair<int, int const>>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<std::tuple<std::pair<int, int const>>>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<mp_list<std::pair<int, int const>, std::pair<long, long const>>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<std::tuple<std::pair<int, int const>, std::pair<long, long const>>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<std::pair<std::pair<int, int const>, std::pair<long, long const>>>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<mp_list<std::pair<int, int const>, std::pair<int, long const>>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<std::tuple<std::pair<int, int const>, std::pair<int, long const>>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<std::pair<std::pair<int, int const>, std::pair<int, long const>>>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<mp_list<mp_list<int>, mp_list<long, long>, mp_list<long long, long long, long long>>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<std::tuple<mp_list<int>, mp_list<long, long>, mp_list<long long, long long, long long>>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<std::tuple<std::tuple<int>, std::pair<long, long>, std::tuple<long long, long long, long long>>>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<mp_list<mp_list<int>, mp_list<long, long>, mp_list<int, long long, long long>>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<std::tuple<mp_list<int>, mp_list<long, long>, mp_list<int, long long, long long>>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_map<std::tuple<std::tuple<int>, std::pair<long, long>, std::tuple<int, long long, long long>>>, mp_false>));

    return boost::report_errors();
}
