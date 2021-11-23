
//  Copyright 2015 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/integral.hpp>
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
    using boost::mp11::mp_contains;
    using boost::mp11::mp_true;
    using boost::mp11::mp_false;

    {
        using L1 = mp_list<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_contains<L1, void>, mp_false>));

        using L2 = mp_list<X1, X2, X3, X2, X3, X3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_contains<L2, void>, mp_false>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_contains<L2, X1>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_contains<L2, X2>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_contains<L2, X3>, mp_true>));
    }

    {
        using L3 = std::tuple<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_contains<L3, void>, mp_false>));

        using L4 = std::tuple<X1, X2, X3, X2, X3, X3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_contains<L4, void>, mp_false>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_contains<L4, X1>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_contains<L4, X2>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_contains<L4, X3>, mp_true>));
    }

    {
        using L5 = std::pair<X1, X2>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_contains<L5, void>, mp_false>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_contains<L5, X1>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_contains<L5, X2>, mp_true>));
    }

    return boost::report_errors();
}
