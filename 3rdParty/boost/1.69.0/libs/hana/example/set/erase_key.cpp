// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/erase_key.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/set.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


int main() {
    constexpr auto xs = hana::make_set(hana::int_c<0>, hana::type_c<int>, hana::type_c<void>);

    BOOST_HANA_CONSTANT_CHECK(hana::erase_key(xs, hana::type_c<int>) == hana::make_set(hana::int_c<0>, hana::type_c<void>));
    BOOST_HANA_CONSTANT_CHECK(hana::erase_key(xs, hana::type_c<char>) == xs);
}
