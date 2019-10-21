
//  Copyright 2015 Peter Dimov.
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

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_set_push_front;

    {
        using L1 = mp_list<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L1>, L1>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L1, void>, mp_list<void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L1, int>, mp_list<int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L1, void, int>, mp_list<void, int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L1, void, int, char[]>, mp_list<void, int, char[]>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L1, void, int, void, int, void, int>, mp_list<void, int>>));
    }

    {
        using L2 = mp_list<void>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L2>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L2, void>, L2>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L2, int>, mp_list<int, void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L2, void, int>, mp_list<int, void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L2, void, void, void, int, int, int>, mp_list<int, void>>));
    }

    {
        using L3 = mp_list<void, int>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L3>, L3>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L3, void>, L3>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L3, int>, L3>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L3, int, int, int, void, void, void>, L3>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L3, void const>, mp_list<void const, void, int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L3, void const, int const>, mp_list<void const, int const, void, int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L3, int, char[], int, char[], void, char[], void, char[]>, mp_list<char[], void, int>>));
    }

    {
        using L4 = mp_list<void, int, char[]>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L4>, L4>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L4, void>, L4>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L4, int>, L4>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L4, char[]>, L4>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L4, void, int, char[], void, int, char[]>, L4>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L4, void const>, mp_list<void const, void, int, char[]>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L4, void const, int const, char const[]>, mp_list<void const, int const, char const[], void, int, char[]>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L4, void, void const, int, int const, char[], char const[]>, mp_list<void const, int const, char const[], void, int, char[]>>));
    }

    //

    {
        using L1 = std::tuple<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L1>, L1>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L1, void>, std::tuple<void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L1, int>, std::tuple<int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L1, void, int>, std::tuple<void, int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L1, void, int, char[]>, std::tuple<void, int, char[]>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L1, void, int, void, int, void, int>, std::tuple<void, int>>));
    }

    {
        using L2 = std::tuple<void>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L2>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L2, void>, L2>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L2, int>, std::tuple<int, void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L2, void, int>, std::tuple<int, void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L2, void, void, void, int, int, int>, std::tuple<int, void>>));
    }

    {
        using L3 = std::tuple<void, int>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L3>, L3>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L3, void>, L3>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L3, int>, L3>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L3, int, int, int, void, void, void>, L3>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L3, void const>, std::tuple<void const, void, int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L3, void const, int const>, std::tuple<void const, int const, void, int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L3, int, char[], int, char[], void, char[], void, char[]>, std::tuple<char[], void, int>>));
    }

    {
        using L4 = std::tuple<void, int, char[]>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L4>, L4>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L4, void>, L4>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L4, int>, L4>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L4, char[]>, L4>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L4, void, int, char[], void, int, char[]>, L4>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L4, void const>, std::tuple<void const, void, int, char[]>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L4, void const, int const, char const[]>, std::tuple<void const, int const, char const[], void, int, char[]>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_set_push_front<L4, void, void const, int, int const, char[], char const[]>, std::tuple<void const, int const, char const[], void, int, char[]>>));
    }

    return boost::report_errors();
}
