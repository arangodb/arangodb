
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/function.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

int main()
{
    using boost::mp11::mp_less;
    using boost::mp11::mp_true;
    using boost::mp11::mp_false;
    using boost::mp11::mp_int;
    using boost::mp11::mp_size_t;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_int<0>, mp_int<-1>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_int<0>, mp_int<0>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_int<0>, mp_int<1>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_int<0>, mp_int<2>>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_int<-1>, mp_int<-2>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_int<-1>, mp_int<-1>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_int<-1>, mp_int<0>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_int<-1>, mp_int<1>>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_int<1>, mp_int<-1>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_int<1>, mp_int<0>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_int<1>, mp_int<1>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_int<1>, mp_int<2>>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_size_t<0>, mp_size_t<0>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_size_t<0>, mp_size_t<1>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_size_t<0>, mp_size_t<2>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_size_t<0>, mp_size_t<3>>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_size_t<1>, mp_size_t<0>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_size_t<1>, mp_size_t<1>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_size_t<1>, mp_size_t<2>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_size_t<1>, mp_size_t<3>>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_size_t<2>, mp_size_t<0>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_size_t<2>, mp_size_t<1>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_size_t<2>, mp_size_t<2>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_size_t<2>, mp_size_t<3>>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_size_t<0>, mp_int<-1>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_size_t<0>, mp_int<0>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_size_t<0>, mp_int<1>>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_int<-1>, mp_size_t<1>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_int<0>, mp_size_t<1>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_int<1>, mp_size_t<1>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_less<mp_int<2>, mp_size_t<1>>, mp_false>));

    return boost::report_errors();
}
