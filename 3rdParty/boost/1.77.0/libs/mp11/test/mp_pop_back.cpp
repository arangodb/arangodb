
// Copyright 2015, 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/core/lightweight_test_trait.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/utility.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_pop_back;

    using L1 = mp_list<void>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pop_back<L1>, mp_list<>>));

    using L2 = mp_list<float, void, int[]>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pop_back<L2>, mp_list<float, void>>));

    using L3 = std::tuple<int>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pop_back<L3>, std::tuple<>>));

    using L4 = std::tuple<char, double>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pop_back<L4>, std::tuple<char>>));

    using boost::mp11::mp_iota_c;
    using boost::mp11::mp_size_t;

    int const N = 137;

    using L6 = mp_iota_c<N>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pop_back<L6>, mp_iota_c<N-1>>));

    using boost::mp11::mp_valid;

    using L7 = mp_list<>;
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_pop_back, L7>));

    return boost::report_errors();
}
