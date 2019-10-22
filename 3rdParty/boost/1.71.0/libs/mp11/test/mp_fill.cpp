
//  Copyright 2015 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/core/lightweight_test_trait.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

struct X1 {};

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_fill;

    using L1 = mp_list<int, void(), float[]>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_fill<L1, X1>, mp_list<X1, X1, X1>>));

    //

    using L2 = std::tuple<int, char, float>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_fill<L2, X1>, std::tuple<X1, X1, X1>>));

    //

    using L3 = std::pair<char, double>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_fill<L3, X1>, std::pair<X1, X1>>));

    //

    return boost::report_errors();
}
