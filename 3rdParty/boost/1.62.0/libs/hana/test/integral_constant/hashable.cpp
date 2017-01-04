// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/integral_constant.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/hashable.hpp>
namespace hana = boost::hana;


int main() {
    constexpr auto ints = hana::make_tuple(
          hana::integral_constant<char, 42>{}
        , hana::integral_constant<signed char, 42>{}
        , hana::integral_constant<signed short, 42>{}
        , hana::integral_constant<signed int, 42>{}
        , hana::integral_constant<signed long, 42>{}
        , hana::integral_constant<unsigned char, 42>{}
        , hana::integral_constant<unsigned short, 42>{}
        , hana::integral_constant<unsigned int, 42>{}
        , hana::integral_constant<unsigned long, 42>{}
    );

    hana::test::TestHashable<hana::integral_constant_tag<void>>{ints};
}
