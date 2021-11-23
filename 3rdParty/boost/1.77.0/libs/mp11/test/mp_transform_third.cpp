
// Copyright 2015, 2017, 2019 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/list.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

struct X1 {};
struct X2 {};
struct X3 {};
struct X4 {};

template<class T> using add_pointer_t = T*;
using Q_add_pointer = boost::mp11::mp_quote_trait<std::add_pointer>;

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_transform_third;
    using boost::mp11::mp_transform_third_q;

    {
        using L3 = mp_list<X1, X2, X3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_third<L3, add_pointer_t>, mp_list<X1, X2, X3*>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_third_q<L3, Q_add_pointer>, mp_list<X1, X2, X3*>>));

        using L4 = mp_list<X1, X2, X3, X4>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_third<L4, add_pointer_t>, mp_list<X1, X2, X3*, X4>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_third_q<L4, Q_add_pointer>, mp_list<X1, X2, X3*, X4>>));
    }

    {
        using L3 = std::tuple<X1, X2, X3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_third<L3, add_pointer_t>, std::tuple<X1, X2, X3*>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_third_q<L3, Q_add_pointer>, std::tuple<X1, X2, X3*>>));

        using L4 = std::tuple<X1, X2, X3, X4>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_third<L4, add_pointer_t>, std::tuple<X1, X2, X3*, X4>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_third_q<L4, Q_add_pointer>, std::tuple<X1, X2, X3*, X4>>));
    }

    return boost::report_errors();
}
