
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/map.hpp>
#include <boost/mp11/list.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

int main()
{
    using boost::mp11::mp_map_keys;
    using boost::mp11::mp_list;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_keys<mp_list<>>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_keys<std::tuple<>>, std::tuple<>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_keys<mp_list<std::pair<int, int const>>>, mp_list<int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_keys<std::tuple<std::pair<int, int const>>>, std::tuple<int>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_keys<mp_list<std::pair<int, int const>, std::pair<long, long const>>>, mp_list<int, long>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_keys<std::tuple<std::pair<int, int const>, std::pair<long, long const>>>, std::tuple<int, long>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_keys<std::pair<std::pair<int, int const>, std::pair<long, long const>>>, std::pair<int, long>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_keys<mp_list<mp_list<int>, mp_list<long, long>, mp_list<long long, long long, long long>>>, mp_list<int, long, long long>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_keys<std::tuple<mp_list<int>, mp_list<long, long>, mp_list<long long, long long, long long>>>, std::tuple<int, long, long long>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_keys<std::tuple<std::tuple<int>, std::pair<long, long>, std::tuple<long long, long long, long long>>>, std::tuple<int, long, long long>>));

    return boost::report_errors();
}
