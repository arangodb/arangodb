// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/equal.hpp>
#include <boost/hana/length.hpp>
#include <boost/hana/maximum.hpp>
#include <boost/hana/ordering.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


int main() {
    static_assert(
        hana::maximum.by(hana::ordering(hana::length), hana::make_tuple(
            hana::make_tuple(),
            hana::make_tuple(1, '2'),
            hana::make_tuple(3.3, nullptr, 4)
        ))
        == hana::make_tuple(3.3, nullptr, 4)
    , "");
}
