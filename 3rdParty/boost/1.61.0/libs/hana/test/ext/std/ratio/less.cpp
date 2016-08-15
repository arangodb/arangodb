// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/ext/std/ratio.hpp>
#include <boost/hana/less.hpp>
#include <boost/hana/not.hpp>

#include <ratio>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::less(
        std::ratio<1>{},
        std::ratio<3>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::less(
        std::ratio<4, 10>{},
        std::ratio<3, 5>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::less(
        std::ratio<3, 5>{},
        std::ratio<4, 10>{}
    )));
}
