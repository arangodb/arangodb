
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
    using boost::mp11::mp_set_intersection;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<mp_list<>>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<mp_list<void>>, mp_list<void>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<mp_list<void, int>>, mp_list<void, int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<mp_list<void, int, float>>, mp_list<void, int, float>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<std::tuple<>, std::pair<float, int>>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<std::tuple<void>, std::pair<float, int>>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<std::tuple<void, int>, std::pair<float, int>>, std::tuple<int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<std::tuple<void, int, float>, std::pair<float, int>>, std::tuple<int, float>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<mp_list<void, int, float>, std::tuple<>>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<mp_list<void, int, float>, std::tuple<int>>, mp_list<int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<mp_list<void, int, float>, std::tuple<float, void>>, mp_list<void, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<mp_list<void, int, float>, std::tuple<float, void, int>>, mp_list<void, int, float>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<std::tuple<>, mp_list<>, std::pair<float, int>>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<std::tuple<void>, mp_list<double>, std::pair<float, int>>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<std::tuple<void, int>, mp_list<double, int>, std::pair<float, int>>, std::tuple<int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<std::tuple<void, int, float>, mp_list<double, int, float>, std::pair<float, int>>, std::tuple<int, float>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<mp_list<void, int, float, double>, std::pair<double, void>, std::tuple<>>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<mp_list<void, int, float, double>, std::pair<double, void>, std::tuple<int>>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<mp_list<void, int, float, double>, std::pair<double, void>, std::tuple<float, void>>, mp_list<void>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_intersection<mp_list<void, int, float, double>, std::pair<double, void>, std::tuple<float, void, int>>, mp_list<void>>));

    return boost::report_errors();
}
