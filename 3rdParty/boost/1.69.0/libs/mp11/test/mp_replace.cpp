
//  Copyright 2015 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

struct X1 {};
struct X2 {};
struct X3 {};

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_replace;

    {
        using L1 = mp_list<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace<L1, void, int[]>, L1>));

        using L2 = mp_list<X1, X2, X3, X2, X3, X3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace<L2, void, int[]>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace<L2, X1, int[]>, mp_list<int[], X2, X3, X2, X3, X3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace<L2, X2, int[]>, mp_list<X1, int[], X3, int[], X3, X3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace<L2, X3, int[]>, mp_list<X1, X2, int[], X2, int[], int[]>>));
    }

    {
        using L1 = std::tuple<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace<L1, void, int[]>, L1>));

        using L2 = std::tuple<X1, X2, X3, X2, X3, X3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace<L2, void, int[]>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace<L2, X1, int[]>, std::tuple<int[], X2, X3, X2, X3, X3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace<L2, X2, int[]>, std::tuple<X1, int[], X3, int[], X3, X3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace<L2, X3, int[]>, std::tuple<X1, X2, int[], X2, int[], int[]>>));
    }

    {
        using L2 = std::pair<X1, X2>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace<L2, void, int[]>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace<L2, X1, int[]>, std::pair<int[], X2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace<L2, X2, int[]>, std::pair<X1, int[]>>));
    }

    return boost::report_errors();
}
