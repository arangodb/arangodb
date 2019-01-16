// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>

#include "matrix/det.hpp"
namespace hana = boost::hana;
using namespace cppcon;


int main() {
    // det
    {
        BOOST_HANA_CONSTEXPR_CHECK(det(matrix(row(1))) == 1);
        BOOST_HANA_CONSTEXPR_CHECK(det(matrix(row(2))) == 2);

        BOOST_HANA_CONSTEXPR_CHECK(det(matrix(row(1, 2), row(3, 4))) == -2);

        BOOST_HANA_CONSTEXPR_CHECK(
            det(matrix(
                row(1, 5, 6),
                row(3, 2, 4),
                row(7, 8, 9)
            ))
            == 51
        );

        BOOST_HANA_CONSTEXPR_CHECK(
            det(matrix(
                row(1, 5, 6, -3),
                row(3, 2, 4, -5),
                row(7, 8, 9, -1),
                row(8, 2, 1, 10)
            )) == 214
        );

        BOOST_HANA_CONSTEXPR_CHECK(
            det(matrix(
                row(1,  5,  6, -3, 92),
                row(3,  2,  4, -5, 13),
                row(7,  8,  9, -1, 0),
                row(8,  2,  1, 10, 41),
                row(3, 12, 92, -7, -4)
            )) == -3115014
        );
    }
}
