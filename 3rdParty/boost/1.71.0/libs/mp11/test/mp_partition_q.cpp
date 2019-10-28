
// Copyright 2015, 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>

struct X1 {};
struct X2 {};
struct X3 {};

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_partition_q;
    using boost::mp11::mp_quote;

    {
        using L1 = mp_list<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_partition_q<L1, mp_quote<std::is_const>>, mp_list<L1, L1>>));

        using L2 = mp_list<X1, X1 const, X1*, X2 const, X2*, X3*>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_partition_q<L2, mp_quote<std::is_volatile>>, mp_list<L1, L2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_partition_q<L2, mp_quote<std::is_const>>, mp_list<mp_list<X1 const, X2 const>, mp_list<X1, X1*, X2*, X3*>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_partition_q<L2, mp_quote<std::is_pointer>>, mp_list<mp_list<X1*, X2*, X3*>, mp_list<X1, X1 const, X2 const>>>));
    }

    {
        using L1 = std::tuple<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_partition_q<L1, mp_quote<std::is_const>>, std::tuple<L1, L1>>));

        using L2 = std::tuple<X1, X1 const, X1*, X2 const, X2*, X3*>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_partition_q<L2, mp_quote<std::is_volatile>>, std::tuple<L1, L2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_partition_q<L2, mp_quote<std::is_const>>, std::tuple<std::tuple<X1 const, X2 const>, std::tuple<X1, X1*, X2*, X3*>>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_partition_q<L2, mp_quote<std::is_pointer>>, std::tuple<std::tuple<X1*, X2*, X3*>, std::tuple<X1, X1 const, X2 const>>>));
    }

    return boost::report_errors();
}
