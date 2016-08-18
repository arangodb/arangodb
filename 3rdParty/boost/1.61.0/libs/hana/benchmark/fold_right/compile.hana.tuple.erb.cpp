// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/fold_right.hpp>
#include <boost/hana/tuple.hpp>


struct f {
    template <typename X, typename State>
    constexpr X operator()(X x, State) const { return x; }
};

struct state { };

template <int i>
struct x { };

int main() {
    constexpr auto tuple = boost::hana::make_tuple(
        <%= (1..input_size).map { |n| "x<#{n}>{}" }.join(', ') %>
    );
    constexpr auto result = boost::hana::fold_right(tuple, state{}, f{});
    (void)result;
}
