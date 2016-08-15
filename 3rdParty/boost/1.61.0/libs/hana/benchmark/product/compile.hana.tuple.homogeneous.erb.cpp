// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/product.hpp>
#include <boost/hana/tuple.hpp>


int main() {
    constexpr auto tuple = boost::hana::make_tuple(
        <%= (1..input_size).to_a.map{ |n| "#{n}ull" }.join(', ') %>
    );
    constexpr auto result = boost::hana::product<unsigned long long>(tuple);
    (void)result;
}
