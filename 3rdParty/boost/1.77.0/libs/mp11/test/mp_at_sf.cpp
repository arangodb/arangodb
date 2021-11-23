
// Copyright 2015, 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/utility.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

using boost::mp11::mp_valid;
using boost::mp11::mp_at;
using boost::mp11::mp_size_t;
using boost::mp11::mp_list;

int main()
{
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_at>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_at, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_at, void, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_at, void, void, void>));

    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_at, void, mp_size_t<0>>));

    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_at, mp_list<>, mp_size_t<0>>));

    BOOST_TEST_TRAIT_TRUE((mp_valid<mp_at, mp_list<void>, mp_size_t<0>>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_at, mp_list<void>, mp_size_t<1>>));

    BOOST_TEST_TRAIT_TRUE((mp_valid<mp_at, mp_list<void, void>, mp_size_t<0>>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<mp_at, mp_list<void, void>, mp_size_t<1>>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_at, mp_list<void, void>, mp_size_t<2>>));

    return boost::report_errors();
}
