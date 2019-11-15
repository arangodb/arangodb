
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
    using boost::mp11::mp_second;

    using L1 = mp_list<void, char[]>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_second<L1>, char[]>));

    using L2 = mp_list<int[], void, float>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_second<L2>, void>));

    using L3 = std::tuple<int, float>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_second<L3>, float>));

    using L4 = std::pair<char, double>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_second<L4>, double>));

    return boost::report_errors();
}
