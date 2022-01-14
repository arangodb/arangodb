
// Copyright 2016, 2017 Peter Dimov.
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
    using boost::mp11::mp_map_erase;
    using boost::mp11::mp_list;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_erase<mp_list<>, void>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_erase<std::tuple<>, int>, std::tuple<>>));

    {
        using M = mp_list<std::pair<int, int const>, std::pair<long, long const>>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_erase<M, char>, M>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_erase<M, int>, mp_list<std::pair<long, long const>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_erase<M, long>, mp_list<std::pair<int, int const>>>));
    }

    {
        using M = std::tuple<std::pair<int, int const>, std::pair<long, long const>>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_erase<M, char>, M>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_erase<M, int>, std::tuple<std::pair<long, long const>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_erase<M, long>, std::tuple<std::pair<int, int const>>>));
    }

    {
        using M = mp_list<mp_list<int>, mp_list<long, long>, mp_list<long long, long long, long long>>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_erase<M, char>, M>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_erase<M, int>, mp_list<mp_list<long, long>, mp_list<long long, long long, long long>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_erase<M, long>, mp_list<mp_list<int>, mp_list<long long, long long, long long>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_erase<M, long long>, mp_list<mp_list<int>, mp_list<long, long>>>));
    }

    {
        using M = std::tuple<mp_list<int>, std::pair<long, long>, std::tuple<long long, long long, long long>>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_erase<M, char>, M>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_erase<M, int>, std::tuple<std::pair<long, long>, std::tuple<long long, long long, long long>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_erase<M, long>, std::tuple<mp_list<int>, std::tuple<long long, long long, long long>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_erase<M, long long>, std::tuple<mp_list<int>, std::pair<long, long>>>));
    }

    return boost::report_errors();
}
