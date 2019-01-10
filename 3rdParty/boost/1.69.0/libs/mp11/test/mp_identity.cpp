
//  Copyright 2015 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/utility.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

struct X {};

int main()
{
    using boost::mp11::mp_identity;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_identity<void const volatile>::type, void const volatile>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_identity<void()>::type, void()>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_identity<int const[]>::type, int const[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_identity<X>::type, X>));

    using boost::mp11::mp_identity_t;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_identity_t<void const volatile>, void const volatile>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_identity_t<void()>, void()>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_identity_t<int const[]>, int const[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_identity_t<X>, X>));

    return boost::report_errors();
}
