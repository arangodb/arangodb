// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/cartesian_product.hpp>
#include <boost/hana/tuple.hpp>


template <int> struct x { };

int main() {
    constexpr auto tuple = boost::hana::make_tuple(
        boost::hana::make_tuple(
            <%= (1..input_size).map{ |i| "x<1#{i}>{}" }.join(', ') %>
        ),
        boost::hana::make_tuple(
            <%= (1..input_size).map{ |i| "x<2#{i}>{}" }.join(', ') %>
        )
    );
    constexpr auto result = boost::hana::cartesian_product(tuple);
    (void)result;
}
