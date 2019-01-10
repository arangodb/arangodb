// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/mult.hpp>
#include <boost/hana/one.hpp>

#include "matrix/comparable.hpp"
#include "matrix/ring.hpp"
namespace hana = boost::hana;
using namespace cppcon;


int main() {
    // mult
    {
        BOOST_HANA_CONSTEXPR_LAMBDA auto a = matrix(
            row(1, 2, 3),
            row(4, 5, 6)
        );

        BOOST_HANA_CONSTEXPR_LAMBDA auto b = matrix(
            row(1, 2),
            row(3, 4),
            row(5, 6)
        );

        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
            hana::mult(a, b),
            matrix(
                row(1*1 + 2*3 + 5*3, 1*2 + 2*4 + 3*6),
                row(4*1 + 3*5 + 5*6, 4*2 + 5*4 + 6*6)
            )
        ));
    }

    // one
    {
        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
            hana::one<Matrix<1, 1>>(),
            matrix(
                row(1)
            )
        ));

        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
            hana::one<Matrix<2, 2>>(),
            matrix(
                row(1, 0),
                row(0, 1)
            )
        ));

        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
            hana::one<Matrix<3, 3>>(),
            matrix(
                row(1, 0, 0),
                row(0, 1, 0),
                row(0, 0, 1)
            )
        ));

        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
            hana::one<Matrix<4, 4>>(),
            matrix(
                row(1, 0, 0, 0),
                row(0, 1, 0, 0),
                row(0, 0, 1, 0),
                row(0, 0, 0, 1)
            )
        ));

        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
            hana::one<Matrix<4, 5>>(),
            matrix(
                row(1, 0, 0, 0, 0),
                row(0, 1, 0, 0, 0),
                row(0, 0, 1, 0, 0),
                row(0, 0, 0, 1, 0)
            )
        ));

        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
            hana::one<Matrix<5, 4>>(),
            matrix(
                row(1, 0, 0, 0),
                row(0, 1, 0, 0),
                row(0, 0, 1, 0),
                row(0, 0, 0, 1),
                row(0, 0, 0, 0)
            )
        ));
    }
}
