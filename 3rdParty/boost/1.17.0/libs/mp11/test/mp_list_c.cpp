
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/list.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

int main()
{
    using boost::mp11::mp_list_c;
    using boost::mp11::mp_list;
    using boost::mp11::mp_int;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_list_c<int>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_list_c<int, 1>, mp_list<mp_int<1>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_list_c<int, 1, 3>, mp_list<mp_int<1>, mp_int<3>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_list_c<int, 1, 3, 5>, mp_list<mp_int<1>, mp_int<3>, mp_int<5>>>));

    return boost::report_errors();
}
