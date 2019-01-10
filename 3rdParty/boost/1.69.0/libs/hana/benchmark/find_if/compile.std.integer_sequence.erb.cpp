// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/bool.hpp>
#include <boost/hana/ext/std/integer_sequence.hpp>
#include <boost/hana/find_if.hpp>
#include <boost/hana/integral_constant.hpp>

#include <utility>
namespace hana = boost::hana;


struct is_last {
    template <typename N>
    constexpr auto operator()(N) const {
        return hana::bool_c<N::value == <%= input_size %>>;
    }
};

int main() {
    auto sequence = std::integer_sequence<
        <%= (["int"] + (1..input_size).to_a).join(', ') %>
    >{};
    auto result = hana::find_if(sequence, is_last{});
    (void)result;
}
