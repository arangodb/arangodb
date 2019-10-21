
//  Copyright 2015 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/integral.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <cstddef>

int main()
{
    using boost::mp11::mp_bool;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bool<false>, std::integral_constant<bool, false>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bool<true>, std::integral_constant<bool, true>>));

    using boost::mp11::mp_true;
    using boost::mp11::mp_false;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_false, std::integral_constant<bool, false>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_true, std::integral_constant<bool, true>>));

    using boost::mp11::mp_int;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_int<0>, std::integral_constant<int, 0>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_int<814>, std::integral_constant<int, 814>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_int<-144>, std::integral_constant<int, -144>>));

    using boost::mp11::mp_size_t;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_size_t<0>, std::integral_constant<std::size_t, 0>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_size_t<1972>, std::integral_constant<std::size_t, 1972>>));

    using boost::mp11::mp_to_bool;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_to_bool<mp_false>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_to_bool<mp_true>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_to_bool<mp_int<0>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_to_bool<mp_int<1>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_to_bool<mp_int<107>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_to_bool<mp_int<-1>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_to_bool<mp_int<-91>>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_to_bool<mp_size_t<0>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_to_bool<mp_size_t<1>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_to_bool<mp_size_t<442>>, mp_true>));

    using boost::mp11::mp_not;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_not<mp_false>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_not<mp_true>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_not<mp_int<0>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_not<mp_int<1>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_not<mp_int<107>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_not<mp_int<-1>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_not<mp_int<-91>>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_not<mp_size_t<0>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_not<mp_size_t<1>>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_not<mp_size_t<442>>, mp_false>));

    return boost::report_errors();
}
