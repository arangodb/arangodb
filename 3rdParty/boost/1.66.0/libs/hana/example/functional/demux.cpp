// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/equal.hpp>
#include <boost/hana/functional/demux.hpp>
#include <boost/hana/functional/placeholder.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;
using hana::_;


constexpr auto f = hana::demux(hana::make_tuple)(
    _ + _,
    _ - _,
    _ * _,
    _ / _
);

static_assert(
    f(10, 4) == hana::make_tuple(
        10 + 4,
        10 - 4,
        10 * 4,
        10 / 4
    )
, "");


int main() { }
