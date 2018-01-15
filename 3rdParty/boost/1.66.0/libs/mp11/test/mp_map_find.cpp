
// Copyright 2016 Peter Dimov.
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
    using boost::mp11::mp_map_find;
    using boost::mp11::mp_list;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_find<mp_list<>, char>, void>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_find<std::tuple<>, int>, void>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_find<mp_list<std::pair<int, int const>, std::pair<long, long const>>, char>, void>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_find<mp_list<std::pair<int, int const>, std::pair<long, long const>>, int>, std::pair<int, int const>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_find<mp_list<std::pair<int, int const>, std::pair<long, long const>>, long>, std::pair<long, long const>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_find<std::tuple<std::pair<int, int const>, std::pair<long, long const>>, char>, void>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_find<std::tuple<std::pair<int, int const>, std::pair<long, long const>>, int>, std::pair<int, int const>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_find<std::tuple<std::pair<int, int const>, std::pair<long, long const>>, long>, std::pair<long, long const>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_find<std::pair<std::pair<int, int const>, std::pair<long, long const>>, char>, void>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_find<std::pair<std::pair<int, int const>, std::pair<long, long const>>, int>, std::pair<int, int const>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_find<std::pair<std::pair<int, int const>, std::pair<long, long const>>, long>, std::pair<long, long const>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_find<mp_list<mp_list<int>, mp_list<long, long>, mp_list<long long, long long, long long>>, char>, void>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_find<mp_list<mp_list<int>, mp_list<long, long>, mp_list<long long, long long, long long>>, int>, mp_list<int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_find<mp_list<mp_list<int>, mp_list<long, long>, mp_list<long long, long long, long long>>, long>, mp_list<long, long>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_find<mp_list<mp_list<int>, mp_list<long, long>, mp_list<long long, long long, long long>>, long long>, mp_list<long long, long long, long long>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_find<std::tuple<mp_list<int>, std::pair<long, long>, std::tuple<long long, long long, long long>>, char>, void>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_find<std::tuple<mp_list<int>, std::pair<long, long>, std::tuple<long long, long long, long long>>, int>, mp_list<int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_find<std::tuple<mp_list<int>, std::pair<long, long>, std::tuple<long long, long long, long long>>, long>, std::pair<long, long>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_find<std::tuple<mp_list<int>, std::pair<long, long>, std::tuple<long long, long long, long long>>, long long>, std::tuple<long long, long long, long long>>));

    return boost::report_errors();
}
