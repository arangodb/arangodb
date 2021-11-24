// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/integral_constant.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/value.hpp>

#include <laws/constant.hpp>
namespace hana = boost::hana;


int main() {
    // value
    static_assert(hana::value(hana::integral_c<int, 0>) == 0, "");
    static_assert(hana::value(hana::integral_c<int, 1>) == 1, "");


    // laws
    hana::test::TestConstant<hana::integral_constant_tag<int>>{
        hana::make_tuple(
            hana::int_c<-10>,
            hana::int_c<-2>,
            hana::int_c<0>,
            hana::int_c<1>,
            hana::int_c<3>,
            hana::int_c<4>
        ),
        hana::tuple_t<int, long, long long>
    };

    hana::test::TestConstant<hana::integral_constant_tag<bool>>{
        hana::make_tuple(
            hana::true_c, hana::false_c
        ),
        hana::tuple_t<bool>
    };
}
