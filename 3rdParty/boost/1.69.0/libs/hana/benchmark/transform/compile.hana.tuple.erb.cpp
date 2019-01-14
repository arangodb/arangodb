// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/transform.hpp>
#include <boost/hana/tuple.hpp>


struct f {
    template <typename X>
    constexpr X operator()(X x) const { return x; }
};

template <int i>
struct x { };

int main() {
    constexpr auto tuple = boost::hana::make_tuple(
        <%= (1..input_size).map { |n| "x<#{n}>{}" }.join(', ') %>
    );
    constexpr auto result = boost::hana::transform(tuple, f{});
    (void)result;
}
