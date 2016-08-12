// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/lexicographical_compare.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


int main() {
    constexpr auto xs = hana::make_tuple(
        <%= (1..input_size).map { |n| n.to_s }.join(', ') %>
    );
    constexpr auto ys = hana::make_tuple(
        <%= (1..input_size).map { |n| (n == input_size ? n + 1 : n).to_s }.join(', ') %>
    );

    constexpr auto result = hana::lexicographical_compare(xs, ys);
    (void)result;
}
