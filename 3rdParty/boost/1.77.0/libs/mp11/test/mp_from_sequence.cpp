
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/mp11/integer_sequence.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

template<class T, T... I> struct S;

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_from_sequence;
    using boost::mp11::mp_int;
    using boost::mp11::mp_size_t;
    using boost::mp11::make_integer_sequence;
    using boost::mp11::make_index_sequence;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_from_sequence<S<int>>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_from_sequence<S<int, 1>>, mp_list<mp_int<1>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_from_sequence<S<int, 1, 3>>, mp_list<mp_int<1>, mp_int<3>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_from_sequence<S<int, 1, 3, 5>>, mp_list<mp_int<1>, mp_int<3>, mp_int<5>>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_from_sequence<make_integer_sequence<int, 0>>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_from_sequence<make_integer_sequence<int, 1>>, mp_list<mp_int<0>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_from_sequence<make_integer_sequence<int, 2>>, mp_list<mp_int<0>, mp_int<1>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_from_sequence<make_integer_sequence<int, 3>>, mp_list<mp_int<0>, mp_int<1>, mp_int<2>>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_from_sequence<make_index_sequence<0>>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_from_sequence<make_index_sequence<1>>, mp_list<mp_size_t<0>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_from_sequence<make_index_sequence<2>>, mp_list<mp_size_t<0>, mp_size_t<1>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_from_sequence<make_index_sequence<3>>, mp_list<mp_size_t<0>, mp_size_t<1>, mp_size_t<2>>>));

    return boost::report_errors();
}
