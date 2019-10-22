// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/functional/flip.hpp>
namespace hana = boost::hana;


BOOST_HANA_CONSTEXPR_LAMBDA auto minus = [](int x, int y, int z = 0) {
    return x - y - z;
};

int main() {
    BOOST_HANA_CONSTEXPR_CHECK(minus(3, 0) == 3 - 0);
    BOOST_HANA_CONSTEXPR_CHECK(hana::flip(minus)(3, 0) == 0 - 3);

    BOOST_HANA_CONSTEXPR_CHECK(minus(3, 0, 1) == 3 - 0 - 1);
    BOOST_HANA_CONSTEXPR_CHECK(hana::flip(minus)(3, 0, 1) == 0 - 3 - 1);
}
