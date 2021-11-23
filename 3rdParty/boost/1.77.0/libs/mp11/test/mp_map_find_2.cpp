
// Copyright 2016, 2020 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/map.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <tuple>

struct X {};
struct Y {};

int main()
{
    using boost::mp11::mp_map_find;

    using L1 = std::tuple<std::tuple<int, int>, std::tuple<long, long>, std::tuple<bool, X>, std::tuple<X, bool>>;

    BOOST_TEST_TRAIT_SAME( mp_map_find<L1, int>, std::tuple<int, int> );
    BOOST_TEST_TRAIT_SAME( mp_map_find<L1, bool>, std::tuple<bool, X> );
    BOOST_TEST_TRAIT_SAME( mp_map_find<L1, X>, std::tuple<X, bool> );

    using L2 = std::tuple<std::tuple<X, Y>, std::tuple<Y, X>>;

    BOOST_TEST_TRAIT_SAME( mp_map_find<L2, X>, std::tuple<X, Y> );
    BOOST_TEST_TRAIT_SAME( mp_map_find<L2, Y>, std::tuple<Y, X> );

    return boost::report_errors();
}
