// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <tuple>


template <int i>
struct x { };

int main() {
    constexpr auto tuple = std::make_tuple(
        <%= (1..input_size).map { |n| "x<#{n}>{}" }.join(', ') %>
    );
    (void)tuple;
}
