// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/adjust_if.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


BOOST_HANA_CONSTEXPR_LAMBDA auto negative = [](auto x) {
    return x < 0;
};

BOOST_HANA_CONSTEXPR_LAMBDA auto negate = [](auto x) {
    return -x;
};

int main() {
    BOOST_HANA_CONSTEXPR_CHECK(
        hana::adjust_if(hana::make_tuple(-3, -2, -1, 0, 1, 2, 3), negative, negate)
                ==
        hana::make_tuple(3, 2, 1, 0, 1, 2, 3)
    );
}
