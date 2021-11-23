// Copyright 2018 Glen Joseph Fernandes
// (glenjofe@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/utility.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

int main()
{
    using boost::mp11::mp_valid;
    using boost::mp11::mp_starts_with;
    using boost::mp11::mp_list;
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_starts_with>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_starts_with,
        void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_starts_with,
        mp_list<> >));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_starts_with,
        mp_list<int> >));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_starts_with,
        void, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_starts_with,
        mp_list<>, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_starts_with,
        mp_list<int>, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_starts_with,
        void, mp_list<> >));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_starts_with,
        void, mp_list<int> >));
    BOOST_TEST_TRAIT_TRUE((mp_valid<mp_starts_with,
        mp_list<>, mp_list<> >));
    BOOST_TEST_TRAIT_TRUE((mp_valid<mp_starts_with,
        mp_list<>, mp_list<int> >));
    BOOST_TEST_TRAIT_TRUE((mp_valid<mp_starts_with,
        mp_list<int>, mp_list<> >));
    BOOST_TEST_TRAIT_TRUE((mp_valid<mp_starts_with,
        mp_list<int>, mp_list<char> >));
    return boost::report_errors();
}
