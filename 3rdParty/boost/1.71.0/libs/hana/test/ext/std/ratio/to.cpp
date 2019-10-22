// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/ratio.hpp>

#include <laws/base.hpp>
#include <support/cnumeric.hpp>

#include <ratio>
#include <utility>
namespace hana = boost::hana;


int main() {
    // Conversion from a Constant to a std::ratio
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to<hana::ext::std::ratio_tag>(cnumeric<int, 0>),
            std::ratio<0>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to<hana::ext::std::ratio_tag>(cnumeric<int, 1>),
            std::ratio<1>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to<hana::ext::std::ratio_tag>(cnumeric<int, 3>),
            std::ratio<3>{}
        ));
    }
}
