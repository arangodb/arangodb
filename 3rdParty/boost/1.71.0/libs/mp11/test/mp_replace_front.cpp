
// Copyright 2015, 2017 Peter Dimov.
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
struct X4 {};

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_replace_front;
    using boost::mp11::mp_replace_first;

    {
        using L1 = mp_list<X1>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_front<L1, void>, mp_list<void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_first<L1, void>, mp_list<void>>));

        using L2 = mp_list<X1, X2>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_front<L2, void>, mp_list<void, X2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_first<L2, void>, mp_list<void, X2>>));

        using L3 = mp_list<X1, X2, X3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_front<L3, void>, mp_list<void, X2, X3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_first<L3, void>, mp_list<void, X2, X3>>));

        using L4 = mp_list<X1, X2, X3, X4>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_front<L4, void>, mp_list<void, X2, X3, X4>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_first<L4, void>, mp_list<void, X2, X3, X4>>));
    }

    {
        using L1 = std::tuple<X1>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_front<L1, void>, std::tuple<void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_first<L1, void>, std::tuple<void>>));

        using L2 = std::tuple<X1, X2>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_front<L2, void>, std::tuple<void, X2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_first<L2, void>, std::tuple<void, X2>>));

        using L3 = std::tuple<X1, X2, X3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_front<L3, void>, std::tuple<void, X2, X3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_first<L3, void>, std::tuple<void, X2, X3>>));

        using L4 = std::tuple<X1, X2, X3, X4>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_front<L4, void>, std::tuple<void, X2, X3, X4>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_first<L4, void>, std::tuple<void, X2, X3, X4>>));
    }

    {
        using L2 = std::pair<X1, X2>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_front<L2, void>, std::pair<void, X2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_first<L2, void>, std::pair<void, X2>>));
    }

    return boost::report_errors();
}
