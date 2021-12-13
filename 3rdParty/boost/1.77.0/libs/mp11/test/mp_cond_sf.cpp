
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/utility.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

int main()
{
    using boost::mp11::mp_cond;
    using boost::mp11::mp_true;
    using boost::mp11::mp_false;
    using boost::mp11::mp_valid;

    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_cond>));

    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_cond, mp_true>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_cond, mp_false>));

    BOOST_TEST_TRAIT_TRUE((mp_valid<mp_cond, mp_true, void>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<mp_cond, mp_true, void, void>));

    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_cond, mp_false, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_cond, mp_false, void, void>));

    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_cond, mp_false, void, mp_true>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<mp_cond, mp_false, void, mp_true, void>));

    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_cond, mp_false, void, mp_false>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_cond, mp_false, void, mp_false, void>));

    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_cond, mp_false, void, mp_false, void, mp_true>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<mp_cond, mp_false, void, mp_false, void, mp_true, void>));

    return boost::report_errors();
}
