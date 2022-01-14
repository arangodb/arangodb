// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/tuple.hpp>
#include <boost/hana/sort.hpp>

int main() {
    constexpr auto tuple = boost::hana::make_tuple(
        <%= (1..input_size).to_a.reverse.map { |n| "boost::hana::int_c<#{n}>" }.join(', ') %>
    );
    constexpr auto result = boost::hana::sort(tuple);
    (void)result;
}
