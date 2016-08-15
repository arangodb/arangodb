// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/ratio.hpp>
#include <boost/hana/mod.hpp>

#include <ratio>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::mod(std::ratio<6>{}, std::ratio<4>{}),
        std::ratio<0>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::mod(std::ratio<3, 4>{}, std::ratio<5, 10>{}),
        std::ratio<0>{}
    ));
}
