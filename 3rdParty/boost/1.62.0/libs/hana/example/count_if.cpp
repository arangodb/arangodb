// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/count_if.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/integral_constant.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/mod.hpp>
#include <boost/hana/not_equal.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>

#include <type_traits>
namespace hana = boost::hana;
using namespace hana::literals;


auto is_odd = [](auto x) {
    return x % 2_c != 0_c;
};

int main() {
    constexpr auto ints = hana::tuple_c<int, 1, 2, 3>;
    BOOST_HANA_CONSTANT_CHECK(hana::count_if(ints, is_odd) == hana::size_c<2>);

    constexpr auto types = hana::tuple_t<int, char, long, short, char, double>;
    BOOST_HANA_CONSTANT_CHECK(hana::count_if(types, hana::trait<std::is_floating_point>) == hana::size_c<1>);
    BOOST_HANA_CONSTANT_CHECK(hana::count_if(types, hana::equal.to(hana::type_c<char>)) == hana::size_c<2>);
    BOOST_HANA_CONSTANT_CHECK(hana::count_if(types, hana::equal.to(hana::type_c<void>)) == hana::size_c<0>);
}
