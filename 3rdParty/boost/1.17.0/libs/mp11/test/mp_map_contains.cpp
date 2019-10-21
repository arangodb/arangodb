
// Copyright 2016 Peter Dimov.
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
    using boost::mp11::mp_map_contains;
    using boost::mp11::mp_list;
    using boost::mp11::mp_true;
    using boost::mp11::mp_false;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_contains<mp_list<>, char>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_contains<std::tuple<>, int>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_contains<mp_list<std::pair<int, int const>, std::pair<long, long const>>, char>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_contains<mp_list<std::pair<int, int const>, std::pair<long, long const>>, int>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_contains<mp_list<std::pair<int, int const>, std::pair<long, long const>>, long>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_contains<std::tuple<std::pair<int, int const>, std::pair<long, long const>>, char>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_contains<std::tuple<std::pair<int, int const>, std::pair<long, long const>>, int>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_contains<std::tuple<std::pair<int, int const>, std::pair<long, long const>>, long>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_contains<std::pair<std::pair<int, int const>, std::pair<long, long const>>, char>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_contains<std::pair<std::pair<int, int const>, std::pair<long, long const>>, int>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_contains<std::pair<std::pair<int, int const>, std::pair<long, long const>>, long>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_contains<mp_list<mp_list<int>, mp_list<long, long>, mp_list<long long, long long, long long>>, char>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_contains<mp_list<mp_list<int>, mp_list<long, long>, mp_list<long long, long long, long long>>, int>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_contains<mp_list<mp_list<int>, mp_list<long, long>, mp_list<long long, long long, long long>>, long>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_contains<mp_list<mp_list<int>, mp_list<long, long>, mp_list<long long, long long, long long>>, long long>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_contains<std::tuple<mp_list<int>, std::pair<long, long>, std::tuple<long long, long long, long long>>, char>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_contains<std::tuple<mp_list<int>, std::pair<long, long>, std::tuple<long long, long long, long long>>, int>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_contains<std::tuple<mp_list<int>, std::pair<long, long>, std::tuple<long long, long long, long long>>, long>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_contains<std::tuple<mp_list<int>, std::pair<long, long>, std::tuple<long long, long long, long long>>, long long>, mp_true>));

    return boost::report_errors();
}
