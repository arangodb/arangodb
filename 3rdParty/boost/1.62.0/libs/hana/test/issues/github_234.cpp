// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/set.hpp>
namespace hana = boost::hana;


int main() {
    {
        auto set = hana::make_set(hana::int_c<1>, hana::integral_c<signed char, 'x'>);

        BOOST_HANA_CONSTANT_CHECK(hana::contains(set, hana::int_c<1>));
        BOOST_HANA_CONSTANT_CHECK(hana::contains(set, hana::integral_c<signed char, 'x'>));
    }

    {
        auto map = hana::make_map(
            hana::make_pair(hana::int_c<1>, 1),
            hana::make_pair(hana::integral_c<signed char, 'x'>, 'x')
        );

        BOOST_HANA_CONSTANT_CHECK(hana::contains(map, hana::int_c<1>));
        BOOST_HANA_CONSTANT_CHECK(hana::contains(map, hana::integral_c<signed char, 'x'>));
    }
}
