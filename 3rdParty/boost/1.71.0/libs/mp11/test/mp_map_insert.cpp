
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
    using boost::mp11::mp_map_insert;
    using boost::mp11::mp_list;
    using boost::mp11::mp_push_back;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<mp_list<>, mp_list<void>>, mp_list<mp_list<void>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<std::tuple<>, std::tuple<int>>, std::tuple<std::tuple<int>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<std::tuple<>, std::pair<int, float>>, std::tuple<std::pair<int, float>>>));

    {
        using M = mp_list<std::pair<int, int const>, std::pair<long, long const>>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<M, mp_list<char>>, mp_push_back<M, mp_list<char>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<M, std::pair<char, char const>>, mp_push_back<M, std::pair<char, char const>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<M, mp_list<int>>, M>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<M, std::pair<long, float>>, M>));
    }

    {
        using M = std::tuple<std::pair<int, int const>, std::pair<long, long const>>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<M, mp_list<char>>, mp_push_back<M, mp_list<char>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<M, std::pair<char, char const>>, mp_push_back<M, std::pair<char, char const>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<M, mp_list<int>>, M>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<M, std::pair<long, float>>, M>));
    }

    {
        using M = mp_list<mp_list<int>, mp_list<long, long>, mp_list<long long, long long, long long>>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<M, mp_list<char>>, mp_push_back<M, mp_list<char>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<M, std::pair<char, char const>>, mp_push_back<M, std::pair<char, char const>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<M, std::tuple<int>>, M>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<M, std::pair<long, float>>, M>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<M, mp_list<long long, float, double>>, M>));
    }

    {
        using M = std::tuple<mp_list<int>, std::pair<long, long>, std::tuple<long long, long long, long long>>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<M, mp_list<char>>, mp_push_back<M, mp_list<char>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<M, std::pair<char, char const>>, mp_push_back<M, std::pair<char, char const>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<M, std::tuple<int>>, M>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<M, std::pair<long, float>>, M>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_map_insert<M, mp_list<long long, float, double>>, M>));
    }

    return boost::report_errors();
}
