
//  Copyright 2015 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/core/lightweight_test_trait.hpp>
#include <boost/mp11/list.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_push_back;

    using L1 = mp_list<>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_push_back<L1>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_push_back<L1, char[1]>, mp_list<char[1]>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_push_back<L1, char[1], char[2]>, mp_list<char[1], char[2]>>));

    using L2 = mp_list<void>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_push_back<L2>, mp_list<void>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_push_back<L2, char[1]>, mp_list<void, char[1]>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_push_back<L2, char[1], char[2]>, mp_list<void, char[1], char[2]>>));

    using L3 = mp_list<int[], void, float>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_push_back<L3>, mp_list<int[], void, float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_push_back<L3, char[1]>, mp_list<int[], void, float, char[1]>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_push_back<L3, char[1], char[2]>, mp_list<int[], void, float, char[1], char[2]>>));

    using L4 = std::tuple<>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_push_back<L4>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_push_back<L4, char>, std::tuple<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_push_back<L4, char, wchar_t>, std::tuple<char, wchar_t>>));

    using L5 = std::tuple<int>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_push_back<L5>, std::tuple<int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_push_back<L5, char>, std::tuple<int, char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_push_back<L5, char, wchar_t>, std::tuple<int, char, wchar_t>>));

    using L6 = std::tuple<int, int>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_push_back<L6>, std::tuple<int, int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_push_back<L6, char>, std::tuple<int, int, char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_push_back<L6, char, wchar_t>, std::tuple<int, int, char, wchar_t>>));

    return boost::report_errors();
}
