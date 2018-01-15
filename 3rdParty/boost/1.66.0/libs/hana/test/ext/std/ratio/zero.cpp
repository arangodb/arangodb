// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/ratio.hpp>
#include <boost/hana/zero.hpp>

#include <ratio>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zero<hana::ext::std::ratio_tag>(),
        std::ratio<0, 1>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zero<hana::ext::std::ratio_tag>(),
        std::ratio<0, 2>{}
    ));
}
