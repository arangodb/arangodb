// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/ratio.hpp>
#include <boost/hana/one.hpp>

#include <ratio>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::one<hana::ext::std::ratio_tag>(),
        std::ratio<1, 1>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::one<hana::ext::std::ratio_tag>(),
        std::ratio<2, 2>{}
    ));
}
