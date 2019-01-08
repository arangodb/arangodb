
//  Copyright 2015 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/core/lightweight_test_trait.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/integral.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_empty;
    using boost::mp11::mp_true;
    using boost::mp11::mp_false;

    using L1 = mp_list<>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_empty<L1>, mp_true>));

    using L2 = mp_list<void>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_empty<L2>, mp_false>));

    using L3 = mp_list<int[], void, float>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_empty<L3>, mp_false>));

    using L4 = std::tuple<>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_empty<L4>, mp_true>));

    using L5 = std::tuple<int>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_empty<L5>, mp_false>));

    using L6 = std::tuple<int, int>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_empty<L6>, mp_false>));

    using L7 = std::pair<char, double>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_empty<L7>, mp_false>));

    using L8 = std::add_const<void()>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_empty<L8>, mp_false>));

    return boost::report_errors();
}
