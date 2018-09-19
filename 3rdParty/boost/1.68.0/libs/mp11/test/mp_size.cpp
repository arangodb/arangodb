
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
    using boost::mp11::mp_size;
    using boost::mp11::mp_size_t;

    using L1 = mp_list<>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_size<L1>, mp_size_t<0>>));

    using L2 = mp_list<void>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_size<L2>, mp_size_t<1>>));

    using L3 = mp_list<int[], void, float>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_size<L3>, mp_size_t<3>>));

    using L4 = std::tuple<>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_size<L4>, mp_size_t<0>>));

    using L5 = std::tuple<int>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_size<L5>, mp_size_t<1>>));

    using L6 = std::tuple<int, int>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_size<L6>, mp_size_t<2>>));

    using L7 = std::pair<char, double>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_size<L7>, mp_size_t<2>>));

    using L8 = std::add_const<void()>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_size<L8>, mp_size_t<1>>));

    return boost::report_errors();
}
