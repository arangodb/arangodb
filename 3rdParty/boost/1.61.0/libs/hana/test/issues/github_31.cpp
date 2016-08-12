// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/any_of.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/find_if.hpp>
#include <boost/hana/fwd/at.hpp>
#include <boost/hana/fwd/drop_front.hpp>
#include <boost/hana/fwd/is_empty.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/optional.hpp>
namespace hana = boost::hana;


// A simple infinite Iterable.
template <int i>
struct counter { };

namespace boost { namespace hana {
    template <int i>
    struct at_impl<counter<i>> {
        template <typename N>
        static constexpr auto apply(counter<i>, N const&) {
            return hana::int_c<i + N::value>;
        }
    };

    template <int i>
    struct drop_front_impl<counter<i>> {
        template <typename N>
        static constexpr auto apply(counter<i>, N) { return counter<i + N::value>{}; }
    };

    template <int i>
    struct is_empty_impl<counter<i>> {
        static constexpr auto apply(counter<i>) { return hana::false_c; }
    };
}}


int main() {
    // find_if and any_of should short-circuit and stop even though the
    // Iterable is infinite.
    BOOST_HANA_CONSTANT_CHECK(hana::any_of(counter<1>{}, hana::equal.to(hana::int_c<4>)));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::find_if(counter<1>{}, hana::equal.to(hana::int_c<4>)),
        hana::just(hana::int_c<4>)
    ));
}
