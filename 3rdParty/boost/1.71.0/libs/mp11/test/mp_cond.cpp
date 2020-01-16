
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

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_cond<mp_true, char[]>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_cond<mp_true, char[], void>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_cond<mp_true, char[], void, void>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_cond<mp_true, char[], void, void, void>, char[]>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_cond<mp_false, int, mp_true, char[]>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_cond<mp_false, int, mp_true, char[], void>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_cond<mp_false, int, mp_true, char[], void, void>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_cond<mp_false, int, mp_true, char[], void, void, void>, char[]>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_cond<mp_false, int, mp_false, float, mp_true, char[]>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_cond<mp_false, int, mp_false, float, mp_true, char[], void>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_cond<mp_false, int, mp_false, float, mp_true, char[], void, void>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_cond<mp_false, int, mp_false, float, mp_true, char[], void, void, void>, char[]>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_cond<mp_false, int, mp_false, float, mp_false, double, mp_true, char[]>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_cond<mp_false, int, mp_false, float, mp_false, double, mp_true, char[], void>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_cond<mp_false, int, mp_false, float, mp_false, double, mp_true, char[], void, void>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_cond<mp_false, int, mp_false, float, mp_false, double, mp_true, char[], void, void, void>, char[]>));

    return boost::report_errors();
}
