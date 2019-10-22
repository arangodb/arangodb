
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/detail/config.hpp>

#if BOOST_MP11_WORKAROUND( BOOST_MP11_GCC, < 40800 )
# pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#include <boost/mp11/function.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

int main()
{
    using boost::mp11::mp_min;
    using boost::mp11::mp_int;
    using boost::mp11::mp_size_t;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_min<mp_int<1>>, mp_int<1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_min<mp_int<2>, mp_int<1>, mp_int<2>, mp_int<3>>, mp_int<1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_min<mp_int<-1>, mp_size_t<1>, mp_int<-2>, mp_size_t<2>>, mp_int<-2>>));

    return boost::report_errors();
}
