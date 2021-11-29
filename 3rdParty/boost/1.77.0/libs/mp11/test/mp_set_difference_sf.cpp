
// Copyright 2019 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/set.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/core/lightweight_test_trait.hpp>

int main()
{
    using boost::mp11::mp_set_difference;
    using boost::mp11::mp_valid;
    using boost::mp11::mp_list;

    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_set_difference>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_set_difference, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_set_difference, void, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_set_difference, void, void, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_set_difference, void, void, void, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_set_difference, void, void, void, void, void>));

    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_set_difference, mp_list<>, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_set_difference, mp_list<>, mp_list<>, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_set_difference, mp_list<>, mp_list<>, mp_list<>, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_set_difference, mp_list<>, mp_list<>, mp_list<>, mp_list<>, void>));

    return boost::report_errors();
}
