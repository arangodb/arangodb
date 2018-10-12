// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/integral_constant.hpp>

#include "matrix/comparable.hpp"
namespace hana = boost::hana;
using namespace cppcon;


int main() {
    // transpose
    {
        BOOST_HANA_CONSTEXPR_LAMBDA auto m = matrix(
            row(1, 2.2, '3'),
            row(4, '5', 6)
        );

        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
            transpose(m),
            matrix(
                row(1, 4),
                row(2.2, '5'),
                row('3', 6)
            )
        ));
    }

    // vector
    {
        BOOST_HANA_CONSTEXPR_LAMBDA auto v = vector(1, '2', hana::int_c<3>, 4.2f);
        BOOST_HANA_CONSTEXPR_CHECK(v.size() == 4ul);
        BOOST_HANA_CONSTEXPR_CHECK(v.nrows() == 4ul);
        BOOST_HANA_CONSTEXPR_CHECK(v.ncolumns() == 1ul);
    }

    // matrix.at
    {
        BOOST_HANA_CONSTEXPR_LAMBDA auto m = matrix(
            row(1, '2', 3),
            row('4', hana::char_c<'5'>, 6),
            row(hana::int_c<7>, '8', 9.3)
        );
        BOOST_HANA_CONSTEXPR_CHECK(m.at(hana::int_c<0>, hana::int_c<0>) == 1);
        BOOST_HANA_CONSTEXPR_CHECK(m.at(hana::int_c<0>, hana::int_c<1>) == '2');
        BOOST_HANA_CONSTEXPR_CHECK(m.at(hana::int_c<0>, hana::int_c<2>) == 3);

        BOOST_HANA_CONSTEXPR_CHECK(m.at(hana::int_c<1>, hana::int_c<0>) == '4');
        BOOST_HANA_CONSTANT_CHECK(m.at(hana::int_c<1>, hana::int_c<1>) == hana::char_c<'5'>);
        BOOST_HANA_CONSTEXPR_CHECK(m.at(hana::int_c<1>, hana::int_c<2>) == 6);

        BOOST_HANA_CONSTANT_CHECK(m.at(hana::int_c<2>, hana::int_c<0>) == hana::int_c<7>);
        BOOST_HANA_CONSTEXPR_CHECK(m.at(hana::int_c<2>, hana::int_c<1>) == '8');
        BOOST_HANA_CONSTEXPR_CHECK(m.at(hana::int_c<2>, hana::int_c<2>) == 9.3);
    }

    // size, ncolumns, nrows
    {
        BOOST_HANA_CONSTEXPR_LAMBDA auto m = matrix(
            row(1, '2', 3),
            row('4', hana::char_c<'5'>, 6)
        );
        BOOST_HANA_CONSTEXPR_CHECK(m.size() == 6ul);
        BOOST_HANA_CONSTEXPR_CHECK(m.ncolumns() == 3ul);
        BOOST_HANA_CONSTEXPR_CHECK(m.nrows() == 2ul);
    }
}
