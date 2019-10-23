
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/utility.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

using boost::mp11::mp_eval_if;
using boost::mp11::mp_eval_if_q;
using boost::mp11::mp_identity_t;
using boost::mp11::mp_valid;
using boost::mp11::mp_quote;

template<class C, class... A> using eval_if = mp_eval_if<C, void, mp_identity_t, A...>;

int main()
{
    BOOST_TEST_TRAIT_TRUE((mp_valid<eval_if, std::true_type>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<eval_if, std::true_type, void>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<eval_if, std::true_type, void, void>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<eval_if, std::true_type, void, void, void>));

    BOOST_TEST_TRAIT_FALSE((mp_valid<eval_if, std::false_type>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<eval_if, std::false_type, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<eval_if, std::false_type, void, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<eval_if, std::false_type, void, void, void>));

    using Qi = mp_quote<mp_identity_t>;

    BOOST_TEST_TRAIT_TRUE((mp_valid<mp_eval_if_q, std::true_type, void, Qi>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<mp_eval_if_q, std::true_type, void, Qi, void>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<mp_eval_if_q, std::true_type, void, Qi, void, void>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<mp_eval_if_q, std::true_type, void, Qi, void, void, void>));

    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_eval_if_q, std::false_type, void, Qi>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<mp_eval_if_q, std::false_type, void, Qi, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_eval_if_q, std::false_type, void, Qi, void, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_eval_if_q, std::false_type, void, Qi, void, void, void>));

    return boost::report_errors();
}
