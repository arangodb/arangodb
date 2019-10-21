// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


struct T { };

int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::make<hana::type_tag>(T{}),
        hana::decltype_(T{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::make<hana::type_tag>(hana::type_c<T>),
        hana::decltype_(hana::type_c<T>)
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::make_type(T{}),
        hana::make<hana::type_tag>(T{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::make_type(hana::type_c<T>),
        hana::make<hana::type_tag>(hana::type_c<T>)
    ));

    // make sure we don't read from non-constexpr variables
    {
        auto t = hana::type_c<T>;
        auto x = 1;
        constexpr auto r1 = hana::make<hana::type_tag>(t); (void)r1;
        constexpr auto r2 = hana::make<hana::type_tag>(x); (void)r2;
    }
}
