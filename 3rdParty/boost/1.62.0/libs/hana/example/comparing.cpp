// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/comparing.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/group.hpp>
#include <boost/hana/length.hpp>
#include <boost/hana/range.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


int main() {
    constexpr auto sequences = hana::make_tuple(
        hana::make_tuple(1, 2, 3),
        hana::make_tuple('x', 'y', 'z'),
        hana::range_c<long, 0, 1>,
        hana::tuple_t<char, int>,
        hana::range_c<int, 0, 2>,
        hana::make_tuple(123.4, nullptr)
    );

    constexpr auto grouped = hana::group.by(hana::comparing(hana::length), sequences);

    static_assert(grouped == hana::make_tuple(
        hana::make_tuple(
            hana::make_tuple(1, 2, 3),
            hana::make_tuple('x', 'y', 'z')
        ),
        hana::make_tuple(
            hana::range_c<long, 0, 1>
        ),
        hana::make_tuple(
            hana::tuple_t<char, int>,
            hana::range_c<int, 0, 2>,
            hana::make_tuple(123.4, nullptr)
        )
    ), "");
}
