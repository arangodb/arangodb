// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/count_if.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/mod.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


struct is_even {
    template <typename N>
    constexpr auto operator()(N n) const {
        return n % hana::int_c<2> == hana::int_c<0>;
    }
};

int main() {
    constexpr auto ints = hana::make_tuple(
        <%= (1..input_size).map { |n| "hana::int_c<#{n}>" }.join(', ') %>
    );
    constexpr auto result = hana::count_if(ints, is_even{});
    (void)result;
}
