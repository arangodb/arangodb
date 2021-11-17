
//  Copyright 2015 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/set.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_set_contains;
    using boost::mp11::mp_true;
    using boost::mp11::mp_false;

    {
        using L1 = mp_list<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L1, void>, mp_false>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L1, int>, mp_false>));

        using L2 = mp_list<void>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L2, void>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L2, int>, mp_false>));

        using L3 = mp_list<void, int>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L3, void>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L3, int>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L3, void const>, mp_false>));

        using L4 = mp_list<void, int, char[]>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L4, void>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L4, int>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L4, char[]>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L4, void const>, mp_false>));
    }

    {
        using L1 = std::tuple<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L1, void>, mp_false>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L1, int>, mp_false>));

        using L2 = std::tuple<void>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L2, void>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L2, int>, mp_false>));

        using L3 = std::tuple<void, int>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L3, void>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L3, int>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L3, void const>, mp_false>));

        using L4 = std::tuple<void, int, char[]>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L4, void>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L4, int>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L4, char[]>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L4, void const>, mp_false>));
    }

    {
        using L3 = std::pair<void, int>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L3, void>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L3, int>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_contains<L3, void const>, mp_false>));
    }

    return boost::report_errors();
}
