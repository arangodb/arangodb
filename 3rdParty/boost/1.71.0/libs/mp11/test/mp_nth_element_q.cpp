
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/mp11/function.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>

int main()
{
    using boost::mp11::mp_nth_element_q;
    using boost::mp11::mp_list_c;
    using boost::mp11::mp_sort_q;
    using boost::mp11::mp_less;
    using boost::mp11::mp_at_c;
    using boost::mp11::mp_size_t;
    using boost::mp11::mp_rename;
    using boost::mp11::mp_quote;

    using Q_less = mp_quote<mp_less>;

    {
        using L1 = mp_list_c<int, 7, 1, 11, 3, 2, 2, 4>;
        using L2 = mp_sort_q<L1, Q_less>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_nth_element_q<L1, mp_size_t<0>, Q_less>, mp_at_c<L2, 0>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_nth_element_q<L1, mp_size_t<1>, Q_less>, mp_at_c<L2, 1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_nth_element_q<L1, mp_size_t<2>, Q_less>, mp_at_c<L2, 2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_nth_element_q<L1, mp_size_t<3>, Q_less>, mp_at_c<L2, 3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_nth_element_q<L1, mp_size_t<4>, Q_less>, mp_at_c<L2, 4>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_nth_element_q<L1, mp_size_t<5>, Q_less>, mp_at_c<L2, 5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_nth_element_q<L1, mp_size_t<6>, Q_less>, mp_at_c<L2, 6>>));
    }

    {
        using L1 = mp_rename<mp_list_c<int, 7, 1, 11, 3, 2, 2, 4>, std::tuple>;
        using L2 = mp_sort_q<L1, Q_less>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_nth_element_q<L1, mp_size_t<0>, Q_less>, mp_at_c<L2, 0>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_nth_element_q<L1, mp_size_t<1>, Q_less>, mp_at_c<L2, 1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_nth_element_q<L1, mp_size_t<2>, Q_less>, mp_at_c<L2, 2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_nth_element_q<L1, mp_size_t<3>, Q_less>, mp_at_c<L2, 3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_nth_element_q<L1, mp_size_t<4>, Q_less>, mp_at_c<L2, 4>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_nth_element_q<L1, mp_size_t<5>, Q_less>, mp_at_c<L2, 5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_nth_element_q<L1, mp_size_t<6>, Q_less>, mp_at_c<L2, 6>>));
    }

    return boost::report_errors();
}
