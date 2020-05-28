
// Copyright 2015, 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/utility.hpp>
#include <boost/mp11/detail/config.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

using boost::mp11::mp_invoke_q;
using boost::mp11::mp_quote;
using boost::mp11::mp_quote_trait;
using boost::mp11::mp_valid;
using boost::mp11::mp_identity;
using boost::mp11::mp_identity_t;

int main()
{
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_invoke_q>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_invoke_q, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_invoke_q, void, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_invoke_q, void, void, void>));

    using Qi = mp_quote<mp_identity_t>;

    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_invoke_q, Qi>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<mp_invoke_q, Qi, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_invoke_q, Qi, void, void>));

    using Qt = mp_quote_trait<mp_identity>;

#if !BOOST_MP11_WORKAROUND( BOOST_MP11_MSVC, <= 1800 )
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_invoke_q, Qt>));
#endif
    BOOST_TEST_TRAIT_TRUE((mp_valid<mp_invoke_q, Qt, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_invoke_q, Qt, void, void>));

    return boost::report_errors();
}
