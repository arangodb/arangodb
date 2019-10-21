
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

struct X1 {};
struct X2 {};
struct X3 {};

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_remove;

    {
        using L1 = mp_list<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_remove<L1, void>, L1>));

        using L2 = mp_list<X1, X2, X3, X2, X3, X3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_remove<L2, void>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_remove<L2, X1>, mp_list<X2, X3, X2, X3, X3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_remove<L2, X2>, mp_list<X1, X3, X3, X3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_remove<L2, X3>, mp_list<X1, X2, X2>>));
    }

    {
        using L1 = std::tuple<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_remove<L1, void>, L1>));

        using L2 = std::tuple<X1, X2, X3, X2, X3, X3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_remove<L2, void>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_remove<L2, X1>, std::tuple<X2, X3, X2, X3, X3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_remove<L2, X2>, std::tuple<X1, X3, X3, X3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_remove<L2, X3>, std::tuple<X1, X2, X2>>));
    }

    return boost::report_errors();
}
