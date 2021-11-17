
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/list.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_is_list;
    using boost::mp11::mp_true;
    using boost::mp11::mp_false;

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_list<void>, mp_false>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_list<int>, mp_false>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_list<char[]>, mp_false>));
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_list<mp_list<>>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_list<mp_list<void>>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_list<mp_list<void, void>>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_list<mp_list<void, void, void>>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_list<mp_list<void, void, void, void>>, mp_true>));
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_list<std::tuple<>>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_list<std::tuple<void>>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_list<std::tuple<void, void>>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_list<std::tuple<void, void, void>>, mp_true>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_list<std::tuple<void, void, void, void>>, mp_true>));
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_is_list<std::pair<void, void>>, mp_true>));
    }

    return boost::report_errors();
}
