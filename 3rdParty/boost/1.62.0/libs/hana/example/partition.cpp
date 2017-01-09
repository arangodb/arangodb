// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/integral_constant.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/mod.hpp>
#include <boost/hana/not_equal.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/partition.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>

#include <type_traits>
namespace hana = boost::hana;


BOOST_HANA_CONSTANT_CHECK(
    hana::partition(hana::tuple_c<int, 1, 2, 3, 4, 5, 6, 7>, [](auto x) {
        return x % hana::int_c<2> != hana::int_c<0>;
    })
    ==
    hana::make_pair(
        hana::tuple_c<int, 1, 3, 5, 7>,
        hana::tuple_c<int, 2, 4, 6>
    )
);

BOOST_HANA_CONSTANT_CHECK(
    hana::partition(hana::tuple_t<void, int, float, char, double>, hana::trait<std::is_floating_point>)
    ==
    hana::make_pair(
        hana::tuple_t<float, double>,
        hana::tuple_t<void, int, char>
    )
);


// partition.by is syntactic sugar
BOOST_HANA_CONSTANT_CHECK(
    hana::partition.by(hana::trait<std::is_floating_point>,
                       hana::tuple_t<void, int, float, char, double>)
    ==
    hana::make_pair(
        hana::tuple_t<float, double>,
        hana::tuple_t<void, int, char>
    )
);

int main() { }
