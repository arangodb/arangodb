
//  Copyright 2015, 2017 Peter Dimov.
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
struct X5 {};

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_replace_at_c;

    {
        using L = mp_list<X1, X2, X3, X4, X5>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_at_c<L, 0, void>, mp_list<void, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_at_c<L, 1, void>, mp_list<X1, void, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_at_c<L, 2, void>, mp_list<X1, X2, void, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_at_c<L, 3, void>, mp_list<X1, X2, X3, void, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_at_c<L, 4, void>, mp_list<X1, X2, X3, X4, void>>));
    }

    {
        using L = std::tuple<X1, X2, X3, X4, X5>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_at_c<L, 0, void>, std::tuple<void, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_at_c<L, 1, void>, std::tuple<X1, void, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_at_c<L, 2, void>, std::tuple<X1, X2, void, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_at_c<L, 3, void>, std::tuple<X1, X2, X3, void, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_at_c<L, 4, void>, std::tuple<X1, X2, X3, X4, void>>));
    }

    {
        using L = std::pair<X1, X2>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_at_c<L, 0, void>, std::pair<void, X2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_at_c<L, 1, void>, std::pair<X1, void>>));
    }

    return boost::report_errors();
}
