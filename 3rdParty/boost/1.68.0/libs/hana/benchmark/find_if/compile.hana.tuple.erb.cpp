// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/bool.hpp>
#include <boost/hana/find_if.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/tuple.hpp>


struct is_last {
    template <typename N>
    constexpr auto operator()(N) const {
        return boost::hana::bool_c<N::value == <%= input_size %>>;
    }
};

int main() {
    constexpr auto tuple = boost::hana::make_tuple(
        <%= (1..input_size).map { |n| "boost::hana::int_c<#{n}>" }.join(', ') %>
    );
    constexpr auto result = boost::hana::find_if(tuple, is_last{});
    (void)result;
}
