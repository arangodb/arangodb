
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
    using boost::mp11::mp_count;
    using boost::mp11::mp_size_t;

    {
        using L1 = mp_list<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count<L1, void>, mp_size_t<0>>));

        using L2 = mp_list<X1, X2, X3, X2, X3, X3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count<L2, void>, mp_size_t<0>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count<L2, X1>, mp_size_t<1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count<L2, X2>, mp_size_t<2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count<L2, X3>, mp_size_t<3>>));
    }

    {
        using L3 = std::tuple<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count<L3, void>, mp_size_t<0>>));

        using L4 = std::tuple<X1, X2, X3, X2, X3, X3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count<L4, void>, mp_size_t<0>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count<L4, X1>, mp_size_t<1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count<L4, X2>, mp_size_t<2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count<L4, X3>, mp_size_t<3>>));
    }

    {
        using L5 = std::pair<X1, X2>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count<L5, void>, mp_size_t<0>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count<L5, X1>, mp_size_t<1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count<L5, X2>, mp_size_t<1>>));
    }

    return boost::report_errors();
}
