// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/integral_constant.hpp>
#include <boost/hana/intersection.hpp>
#include <boost/hana/set.hpp>
namespace hana = boost::hana;


int main() {
    <% half = input_size / 2 %>
    constexpr auto xs = hana::make_set(
        <%= (1..half).map { |n| "hana::int_c<#{n}>" }.join(', ') %>
    );

    constexpr auto ys = hana::make_set(
        <%= (half..input_size).map { |n| "hana::int_c<#{n}>" }.join(', ') %>
    );

    constexpr auto result = hana::intersection(xs, ys);
    (void)result;
}
