
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/detail/config.hpp>

#if BOOST_MP11_MSVC
# pragma warning( disable: 4503 ) // decorated name length exceeded
#endif

#include <boost/mp11/function.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
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

    using boost::mp11::mp_repeat_c;
    using boost::mp11::mp_list;
    using boost::mp11::mp_apply;

    int const N = 1089;

    using L = mp_repeat_c<mp_list<void>, N>;
    using R = mp_apply<mp_same, L>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<R, mp_true>));

    return boost::report_errors();
}
