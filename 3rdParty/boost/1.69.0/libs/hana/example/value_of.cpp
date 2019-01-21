// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/equal.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/value.hpp>
namespace hana = boost::hana;


int main() {
    constexpr auto xs = hana::tuple_c<int, 1, 2, 3, 4, 5>;
    constexpr auto vs = hana::transform(xs, hana::value_of);
    static_assert(vs == hana::make_tuple(1, 2, 3, 4, 5), "");
}
