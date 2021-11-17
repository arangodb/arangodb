
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
    using boost::mp11::mp_replace_second;

    {
        using L2 = mp_list<X1, X2>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_second<L2, void>, mp_list<X1, void>>));

        using L3 = mp_list<X1, X2, X3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_second<L3, void>, mp_list<X1, void, X3>>));

        using L4 = mp_list<X1, X2, X3, X4>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_second<L4, void>, mp_list<X1, void, X3, X4>>));
    }

    {
        using L2 = std::tuple<X1, X2>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_second<L2, void>, std::tuple<X1, void>>));

        using L3 = std::tuple<X1, X2, X3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_second<L3, void>, std::tuple<X1, void, X3>>));

        using L4 = std::tuple<X1, X2, X3, X4>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_second<L4, void>, std::tuple<X1, void, X3, X4>>));
    }

    {
        using L2 = std::pair<X1, X2>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_second<L2, void>, std::pair<X1, void>>));
    }

    return boost::report_errors();
}
