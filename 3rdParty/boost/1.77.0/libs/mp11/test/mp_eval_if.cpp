
// Copyright 2015, 2017 Peter Dimov.
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
    using boost::mp11::mp_eval_if_c;
    using boost::mp11::mp_identity;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_eval_if_c<true, char[], mp_identity, void, void, void>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_eval_if_c<false, char[], mp_identity, void()>, mp_identity<void()>>));

    using boost::mp11::mp_eval_if;
    using boost::mp11::mp_eval_if_q;
    using boost::mp11::mp_quote;

    using qt_identity = mp_quote<mp_identity>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_eval_if<std::true_type, char[], mp_identity, void, void, void>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_eval_if<std::false_type, char[], mp_identity, void()>, mp_identity<void()>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_eval_if_q<std::true_type, char[], qt_identity, void, void, void>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_eval_if_q<std::false_type, char[], qt_identity, void()>, mp_identity<void()>>));

    using boost::mp11::mp_int;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_eval_if<mp_int<-7>, char[], mp_identity, void, void, void>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_eval_if<mp_int<0>, char[], mp_identity, void()>, mp_identity<void()>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_eval_if_q<mp_int<-7>, char[], qt_identity, void, void, void>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_eval_if_q<mp_int<0>, char[], qt_identity, void()>, mp_identity<void()>>));

    using boost::mp11::mp_size_t;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_eval_if<mp_size_t<14>, char[], mp_identity, void, void, void>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_eval_if<mp_size_t<0>, char[], mp_identity, void()>, mp_identity<void()>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_eval_if_q<mp_size_t<14>, char[], qt_identity, void, void, void>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_eval_if_q<mp_size_t<0>, char[], qt_identity, void()>, mp_identity<void()>>));

    return boost::report_errors();
}
