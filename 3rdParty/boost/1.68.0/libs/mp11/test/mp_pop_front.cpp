
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
    using boost::mp11::mp_pop_front;
    using boost::mp11::mp_rest;

    using L1 = mp_list<void>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pop_front<L1>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rest<L1>, mp_list<>>));

    using L2 = mp_list<int[], void(), float>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pop_front<L2>, mp_list<void(), float>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rest<L2>, mp_list<void(), float>>));

    using L3 = std::tuple<int>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pop_front<L3>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rest<L3>, std::tuple<>>));

    using L4 = std::tuple<char, double>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_pop_front<L4>, std::tuple<double>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rest<L4>, std::tuple<double>>));

    return boost::report_errors();
}
