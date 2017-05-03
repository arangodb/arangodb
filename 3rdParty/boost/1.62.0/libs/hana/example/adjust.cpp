// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/adjust.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


BOOST_HANA_CONSTEXPR_LAMBDA auto negate = [](auto x) {
    return -x;
};

int main() {
    BOOST_HANA_CONSTEXPR_CHECK(
        hana::adjust(hana::make_tuple(1, 4, 9, 2, 3, 4), 4, negate)
        ==
        hana::make_tuple(1, -4, 9, 2, 3, -4)
    );
}
