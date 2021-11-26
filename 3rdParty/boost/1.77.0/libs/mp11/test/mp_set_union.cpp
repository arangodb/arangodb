
// Copyright 2019 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/set.hpp>
#include <boost/mp11/list.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_set_union;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<>, mp_list<>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<mp_list<>>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<mp_list<void>>, mp_list<void>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<mp_list<void, int>>, mp_list<void, int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<mp_list<void, int, float>>, mp_list<void, int, float>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<std::tuple<>>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<std::tuple<void>>, std::tuple<void>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<std::tuple<void, int>>, std::tuple<void, int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<std::tuple<void, int, float>>, std::tuple<void, int, float>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<mp_list<>, std::pair<float, int>>, mp_list<float, int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<mp_list<void>, std::pair<float, int>>, mp_list<void, float, int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<mp_list<void, int>, std::pair<float, int>>, mp_list<void, int, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<mp_list<void, int, float>, std::pair<float, int>>, mp_list<void, int, float>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<std::tuple<>, mp_list<>, std::pair<float, int>>, std::tuple<float, int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<std::tuple<void>, mp_list<double>, std::pair<float, int>>, std::tuple<void, double, float, int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<std::tuple<void, int>, mp_list<double, int>, std::pair<float, int>>, std::tuple<void, int, double, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<std::tuple<void, int, float>, mp_list<double, int, float>, std::pair<float, int>>, std::tuple<void, int, float, double>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<mp_list<>, std::pair<float, float>, std::pair<int, int>>, mp_list<float, int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<mp_list<void>, std::pair<float, float>, std::pair<int, int>>, mp_list<void, float, int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<mp_list<void, int>, std::pair<float, float>, std::pair<int, int>>, mp_list<void, int, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<mp_list<void, int, float>, std::pair<float, float>, std::pair<int, int>>, mp_list<void, int, float>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<std::tuple<>, std::pair<float, float>, std::pair<int, int>, std::pair<void, void>>, std::tuple<float, int, void>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<std::tuple<void>, std::pair<float, float>, std::pair<int, int>, std::pair<void, void>>, std::tuple<void, float, int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<std::tuple<void, int>, std::pair<float, float>, std::pair<int, int>, std::pair<void, void>>, std::tuple<void, int, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_union<std::tuple<void, int, float>, std::pair<float, float>, std::pair<int, int>, std::pair<void, void>>, std::tuple<void, int, float>>));

    return boost::report_errors();
}
