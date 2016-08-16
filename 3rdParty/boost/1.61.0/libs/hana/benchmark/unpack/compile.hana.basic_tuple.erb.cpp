// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/basic_tuple.hpp>
#include <boost/hana/unpack.hpp>
namespace hana = boost::hana;


struct f {
    template <typename ...T>
    constexpr void operator()(T const& ...) const { }
};

template <int i>
struct x { };

int main() {
    constexpr auto tuple = hana::make_basic_tuple(
        <%= (1..input_size).map { |n| "x<#{n}>{}" }.join(', ') %>
    );
    hana::unpack(tuple, f{});
}
