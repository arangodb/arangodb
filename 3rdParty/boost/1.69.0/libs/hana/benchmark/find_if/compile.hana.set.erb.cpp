// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/bool.hpp>
#include <boost/hana/find_if.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/set.hpp>
namespace hana = boost::hana;


struct is_last {
    template <typename N>
    constexpr auto operator()(N) const {
        return hana::bool_c<N::value == <%= input_size %>>;
    }
};

int main() {
    constexpr auto set = hana::make_set(
        <%= (1..input_size).map { |n| "hana::int_c<#{n}>" }.join(', ') %>
    );
    constexpr auto result = hana::find_if(set, is_last{});
    (void)result;
}
