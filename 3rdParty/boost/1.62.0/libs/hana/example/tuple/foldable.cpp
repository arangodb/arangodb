// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/fold_right.hpp>
#include <boost/hana/if.hpp>
#include <boost/hana/less.hpp>
#include <boost/hana/prepend.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;
using namespace hana::literals;


int main() {
    constexpr auto numbers = hana::tuple_c<int, 5, -1, 0, -7, -2, 0, -5, 4>;
    constexpr auto negatives = hana::tuple_c<int, -1, -7, -2, -5>;

    BOOST_HANA_CONSTEXPR_LAMBDA auto keep_negatives = [](auto n, auto acc) {
        return hana::if_(n < 0_c, hana::prepend(acc, n), acc);
    };

    BOOST_HANA_CONSTANT_CHECK(hana::fold_right(numbers, hana::tuple_c<int>, keep_negatives) == negatives);
}
