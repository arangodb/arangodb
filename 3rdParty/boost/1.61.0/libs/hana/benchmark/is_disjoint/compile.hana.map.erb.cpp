// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/integral_constant.hpp>
#include <boost/hana/is_disjoint.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/pair.hpp>
namespace hana = boost::hana;


int main() {
    constexpr auto even = hana::make_map(
        <%= (1..input_size).map { |n|
            "hana::make_pair(hana::int_c<#{n*2}>, hana::int_c<#{n*2}>)"
        }.join(', ') %>
    );
    constexpr auto odd = hana::make_map(
        <%= (1..input_size).map { |n|
            "hana::make_pair(hana::int_c<#{n*2+1}>, hana::int_c<#{n*2+1}>)"
        }.join(', ') %>
    );
    hana::is_disjoint(even, odd);
}
