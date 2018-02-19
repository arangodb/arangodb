
//  Copyright 2015 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/core/lightweight_test_trait.hpp>
#include <boost/mp11/list.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_front;
    using boost::mp11::mp_first;

    using L1 = mp_list<void>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_front<L1>, void>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_first<L1>, void>));

    using L2 = mp_list<int[], void, float>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_front<L2>, int[]>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_first<L2>, int[]>));

    using L3 = std::tuple<int>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_front<L3>, int>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_first<L3>, int>));

    using L4 = std::pair<char, double>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_front<L4>, char>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_first<L4>, char>));

    using L5 = std::add_const<void()>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_front<L5>, void()>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_first<L5>, void()>));

    return boost::report_errors();
}
