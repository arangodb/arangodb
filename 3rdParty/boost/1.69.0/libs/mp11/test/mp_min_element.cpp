
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>

using boost::mp11::mp_bool;

template<class T, class U> using sizeof_less = mp_bool<(sizeof(T) < sizeof(U))>;

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_min_element;

    {
        using L1 = mp_list<void>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_min_element<L1, sizeof_less>, void>));

        using L2 = mp_list<char[2], char[4], char[3], char[1]>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_min_element<L2, sizeof_less>, char[1]>));

        using L3 = mp_list<char[2], char[4], char[2], char[3], char[1], char[2], char[1]>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_min_element<L3, sizeof_less>, char[1]>));
    }

    {
        using L1 = std::tuple<void>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_min_element<L1, sizeof_less>, void>));

        using L2 = std::tuple<char[2], char[4], char[3], char[1]>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_min_element<L2, sizeof_less>, char[1]>));

        using L3 = std::tuple<char[2], char[4], char[2], char[3], char[1], char[2], char[1]>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_min_element<L3, sizeof_less>, char[1]>));
    }

    return boost::report_errors();
}
