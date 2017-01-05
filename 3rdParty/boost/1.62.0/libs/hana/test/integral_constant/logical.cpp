// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/eval_if.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
#include <laws/logical.hpp>
namespace hana = boost::hana;


int main() {
    // eval_if
    {
        auto t = [](auto) { return hana::test::ct_eq<3>{}; };
        auto e = [](auto) { return hana::test::ct_eq<4>{}; };

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::eval_if(hana::true_c, t, e),
            hana::test::ct_eq<3>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::eval_if(hana::false_c, t, e),
            hana::test::ct_eq<4>{}
        ));
    }

    // not_
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::not_(hana::true_c),
            hana::false_c
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::not_(hana::false_c),
            hana::true_c
        ));
    }

    // laws
    hana::test::TestLogical<hana::integral_constant_tag<int>>{hana::make_tuple(
        hana::int_c<-2>, hana::int_c<0>, hana::int_c<1>, hana::int_c<3>
    )};

    hana::test::TestLogical<hana::integral_constant_tag<bool>>{hana::make_tuple(
        hana::false_c, hana::true_c
    )};
}
