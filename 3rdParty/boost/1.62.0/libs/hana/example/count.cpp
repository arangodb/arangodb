// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/count.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


int main() {
    constexpr auto ints = hana::tuple_c<int, 1, 2, 3, 2, 2, 4, 2>;
    BOOST_HANA_CONSTANT_CHECK(hana::count(ints, hana::int_c<2>) == hana::size_c<4>);
    static_assert(hana::count(ints, 2) == 4, "");


    constexpr auto types = hana::tuple_t<int, char, long, short, char, double>;
    BOOST_HANA_CONSTANT_CHECK(hana::count(types, hana::type_c<char>) == hana::size_c<2>);
}
