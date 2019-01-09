// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/if.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/minus.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/unfold_right.hpp>
namespace hana = boost::hana;


BOOST_HANA_CONSTANT_CHECK(
    hana::unfold_right<hana::tuple_tag>(hana::int_c<10>, [](auto x) {
        return hana::if_(x == hana::int_c<0>,
            hana::nothing,
            hana::just(hana::make_pair(x, x - hana::int_c<1>))
        );
    })
    ==
    hana::tuple_c<int, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1>
);

int main() { }
