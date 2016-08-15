// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/less.hpp>
#include <boost/hana/lexicographical_compare.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


int main() {
    // works with elements whose `less` does not return a Constant
    {
        constexpr auto xs = hana::make_tuple(1, 2, 3, 4);
        constexpr auto ys = hana::make_tuple(1, 6, 3, 4);
        static_assert(hana::lexicographical_compare(xs, ys), "");
    }

    // and with those that do
    {
        auto xs = hana::make_tuple(hana::int_c<1>, hana::int_c<2>, hana::int_c<3>);
        auto ys = hana::make_tuple(hana::int_c<1>, hana::int_c<5>, hana::int_c<3>);
        BOOST_HANA_CONSTANT_CHECK(hana::lexicographical_compare(xs, ys));
    }

    // it also accepts a custom predicate
    {
        auto xs = hana::make_tuple(hana::type_c<int>, hana::type_c<char>, hana::type_c<void*>);
        auto ys = hana::make_tuple(hana::type_c<int>, hana::type_c<long>, hana::type_c<void*>);
        BOOST_HANA_CONSTANT_CHECK(
            hana::lexicographical_compare(xs, ys, [](auto t, auto u) {
                return hana::sizeof_(t) < hana::sizeof_(u);
            })
        );
    }
}
