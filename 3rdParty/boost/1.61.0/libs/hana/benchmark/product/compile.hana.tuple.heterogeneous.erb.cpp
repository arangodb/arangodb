// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/product.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/integral_constant.hpp>
namespace hana = boost::hana;


int main() {
    constexpr auto tuple = hana::make_tuple(<%=
        (1..input_size).to_a.map{ |n| "hana::ullong_c<#{n}>" }.join(', ')
    %>);
    constexpr auto result = hana::product<
        hana::integral_constant_tag<unsigned long long>
    >(tuple);
    (void)result;
}
