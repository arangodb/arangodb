
//  Copyright 2015 Peter Dimov.
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
    using boost::mp11::mp_if_c;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_if_c<true, char[], void()>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_if_c<false, char[], void()>, void()>));

    using boost::mp11::mp_if;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_if<std::true_type, char[], void()>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_if<std::false_type, char[], void()>, void()>));

    using boost::mp11::mp_int;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_if<mp_int<-7>, char[], void()>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_if<mp_int<0>, char[], void()>, void()>));

    using boost::mp11::mp_size_t;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_if<mp_size_t<14>, char[], void()>, char[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_if<mp_size_t<0>, char[], void()>, void()>));

    return boost::report_errors();
}
