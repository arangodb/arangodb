// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>

#include <laws/comparable.hpp>
#include <laws/hashable.hpp>
namespace hana = boost::hana;


struct T;

int main() {
    auto types = hana::make_tuple(
        hana::type_c<T>,
        hana::type_c<T*>,
        hana::type_c<T&>,
        hana::type_c<T&&>,
        hana::type_c<T const>,
        hana::type_c<T volatile>,
        hana::type_c<T const volatile>
    );

    hana::test::TestComparable<hana::type_tag>{types};
    hana::test::TestHashable<hana::type_tag>{types};
}
