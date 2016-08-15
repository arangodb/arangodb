// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/product.hpp>
#include <boost/hana/range.hpp>


int main() {
    constexpr auto range = boost::hana::range_c<
        unsigned long long, 1, <%= input_size %>
    >;
    constexpr auto result = boost::hana::product<>(range);
    (void)result;
}
