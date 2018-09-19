// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/functional/placeholder.hpp>
#include <boost/hana/integral_constant.hpp>

#include "matrix/comparable.hpp"
#include "matrix/functor.hpp"
namespace hana = boost::hana;
using namespace cppcon;


int main() {
    // transform
    {
        BOOST_HANA_CONSTEXPR_LAMBDA auto m = matrix(
            row(1,              hana::int_c<2>, 3),
            row(hana::int_c<4>, 5,              6),
            row(7,              8,              hana::int_c<9>)
        );

        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
            hana::transform(m, hana::_ + hana::int_c<1>),
            matrix(
                row(2,              hana::int_c<3>, 4),
                row(hana::int_c<5>, 6,              7),
                row(8,              9,              hana::int_c<10>)
            )
        ));
    }
}
