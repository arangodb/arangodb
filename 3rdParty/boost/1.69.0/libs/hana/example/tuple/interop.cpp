// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/array.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/range.hpp>
#include <boost/hana/tuple.hpp>

#include <array>
#include <tuple>
namespace hana = boost::hana;


int main() {
    static_assert(
        hana::to_tuple(std::make_tuple(1, '2', 3.3)) == hana::make_tuple(1, '2', 3.3)
    , "");

    BOOST_HANA_CONSTANT_CHECK(
        hana::to_tuple(hana::make_range(hana::int_c<1>, hana::int_c<4>))
            ==
        hana::make_tuple(hana::int_c<1>, hana::int_c<2>, hana::int_c<3>)
    );

    // std::array's operator[] is not constexpr, so we can't use static_assert
    BOOST_HANA_CONSTEXPR_CHECK(
        hana::to_tuple(std::array<int, 3>{{1, 2, 3}}) == hana::make_tuple(1, 2, 3)
    );
}
