// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/any_of.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/ext/std/integral_constant.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/mod.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/not_equal.hpp>
#include <boost/hana/traits.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>

#include <type_traits>
namespace hana = boost::hana;
using namespace hana::literals;


BOOST_HANA_CONSTEXPR_LAMBDA auto is_odd = [](auto x) {
    return x % 2_c != 0_c;
};

int main() {
    BOOST_HANA_CONSTEXPR_CHECK(hana::any_of(hana::make_tuple(1, 2), is_odd));
    BOOST_HANA_CONSTANT_CHECK(!hana::any_of(hana::make_tuple(2_c, 4_c), is_odd));

    BOOST_HANA_CONSTANT_CHECK(
      hana::any_of(hana::make_tuple(hana::type<void>{}, hana::type<char&>{}),
                   hana::traits::is_void)
    );

    BOOST_HANA_CONSTANT_CHECK(
      !hana::any_of(hana::make_tuple(hana::type<void>{}, hana::type<char&>{}),
                    hana::traits::is_integral)
    );
}
