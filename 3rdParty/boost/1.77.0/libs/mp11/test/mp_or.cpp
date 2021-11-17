
// Copyright 2015-2017 Peter Dimov.
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
    using boost::mp11::mp_or;
    using boost::mp11::mp_true;
    using boost::mp11::mp_false;
    using boost::mp11::mp_int;
    using boost::mp11::mp_size_t;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_true>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_false>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_int<-7>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_int<0>>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_size_t<7>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_size_t<0>>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_true, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_false, mp_true>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_false, mp_false>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_int<0>, mp_int<0>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_int<0>, mp_int<5>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_int<-4>, void>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_size_t<7>, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_size_t<0>, mp_size_t<4>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_size_t<0>, mp_size_t<0>>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_true, void, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_false, mp_true, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_false, mp_false, mp_true>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_false, mp_false, mp_false>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_true, void, void, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_false, mp_true, void, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_false, mp_false, mp_true, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_false, mp_false, mp_false, mp_true>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_false, mp_false, mp_false, mp_false>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_int<1>, void, void, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_int<0>, mp_int<2>, void, void, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_int<0>, mp_int<0>, mp_int<3>, void, void, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_int<0>, mp_int<0>, mp_int<0>, mp_int<4>, void, void, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_int<0>, mp_int<0>, mp_int<0>, mp_int<0>, mp_int<0>>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_size_t<1>, void, void, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_size_t<0>, mp_size_t<2>, void, void, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_size_t<0>, mp_size_t<0>, mp_size_t<3>, void, void, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_size_t<0>, mp_size_t<0>, mp_size_t<0>, mp_size_t<4>, void, void, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_size_t<0>, mp_size_t<0>, mp_size_t<0>, mp_size_t<0>, mp_size_t<0>>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_size_t<0>, mp_int<0>, mp_false, mp_size_t<141>, void, void, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_or<mp_size_t<0>, mp_int<0>, mp_false>, mp_false>));

    return boost::report_errors();
}
