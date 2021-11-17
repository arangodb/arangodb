// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/not.hpp>

#include "matrix/comparable.hpp"
namespace hana = boost::hana;
using namespace cppcon;


int main() {
    BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
        matrix(row(1, 2)),
        matrix(row(1, 2))
    ));
    BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::equal(
        matrix(row(1, 2)),
        matrix(row(1, 5))
    )));

    BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
        matrix(row(1, 2),
               row(3, 4)),
        matrix(row(1, 2),
               row(3, 4))
    ));
    BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::equal(
        matrix(row(1, 2),
               row(3, 4)),
        matrix(row(1, 2),
               row(0, 4))
    )));
    BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::equal(
        matrix(row(1, 2),
               row(3, 4)),
        matrix(row(0, 2),
               row(3, 4))
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        matrix(row(1),
               row(2)),
        matrix(row(3, 4),
               row(5, 6))
    )));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        matrix(row(1),
               row(2)),
        matrix(row(3, 4))
    )));
}

