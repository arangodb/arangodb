// Copyright Jason Rice 2015
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/bool.hpp>
#include <boost/hana/find_if.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/map.hpp>
namespace hana = boost::hana;


struct is_last {
    template <typename N>
    constexpr auto operator()(N) const {
        return hana::bool_c<N::value == <%= input_size %>>;
    }
};

struct undefined {};

int main() {
    constexpr auto map = hana::make_map(
        <%= (1..input_size).map { |n|
            "hana::make_pair(hana::int_c<#{n}>, undefined{})"
        }.join(', ') %>
    );
    constexpr auto result = hana::find_if(map, is_last{});
    (void)result;
}
