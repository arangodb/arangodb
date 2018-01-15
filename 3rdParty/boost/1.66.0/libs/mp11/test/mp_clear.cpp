
// Copyright 2015, 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/list.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_clear;

    using L1 = mp_list<int, void(), float[]>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_clear<L1>, mp_list<>>));

    //

    using L2 = std::tuple<int, char, float>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_clear<L2>, std::tuple<>>));

    //

    return boost::report_errors();
}
