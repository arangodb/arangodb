
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/function.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

int main()
{
    using boost::mp11::mp_same;
    using boost::mp11::mp_true;
    using boost::mp11::mp_false;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_same<>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_same<void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_same<void, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_same<void, void, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_same<void, void, void, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_same<void, void, void, void, void>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_same<void, int>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_same<void, void, int>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_same<void, void, void, int>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_same<void, void, void, void, int>, mp_false>));

    return boost::report_errors();
}
