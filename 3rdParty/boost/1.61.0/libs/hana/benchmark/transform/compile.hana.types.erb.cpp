/*
@copyright Louis Dionne 2015
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
 */

#include <boost/hana/experimental/types.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


template <typename>
struct f { struct type; };

template <int> struct x;


int main() {
    constexpr auto types = hana::experimental::types<
        <%= (1..input_size).map { |i| "x<#{i}>" }.join(', ') %>
    >{};

    constexpr auto result = hana::transform(types, hana::metafunction<f>);
    (void)result;
}
