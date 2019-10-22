// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include "minimal.hpp"

#include <boost/hana/and.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/eval_if.hpp>
#include <boost/hana/if.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/not_equal.hpp>
#include <boost/hana/or.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/while.hpp>

#include <laws/base.hpp>
#include <laws/logical.hpp>
namespace hana = boost::hana;


struct invalid { };

int main() {
    constexpr auto bools = hana::make_tuple(
        minimal_constant<bool, false>{},
        minimal_constant<bool, true>{}
    );

    hana::test::TestLogical<minimal_constant_tag<bool>>{bools};


    constexpr auto true_ = minimal_constant<bool, true>{};
    constexpr auto false_ = minimal_constant<bool, false>{};

    // not_
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(hana::not_(true_), false_));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(hana::not_(false_), true_));
    }

    // and_
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::and_(true_),
            true_
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::and_(false_),
            false_
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::and_(true_, true_),
            true_
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::and_(true_, false_),
            false_
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::and_(false_, invalid{}),
            false_
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::and_(true_, true_, true_),
            true_
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::and_(true_, true_, false_),
            false_
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::and_(true_, false_, invalid{}),
            false_
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::and_(false_, invalid{}, invalid{}),
            false_
        ));
    }

    // or_
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::or_(true_),
            true_
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::or_(false_),
            false_
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::or_(false_, false_),
            false_
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::or_(false_, true_),
            true_
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::or_(true_, invalid{}),
            true_
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::or_(false_, false_, false_),
            false_
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::or_(false_, false_, true_),
            true_
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::or_(false_, true_, invalid{}),
            true_
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::or_(true_, invalid{}, invalid{}),
            true_
        ));
    }

    // if_
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::if_(true_, hana::test::ct_eq<3>{}, hana::test::ct_eq<4>{}),
            hana::test::ct_eq<3>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::if_(false_, hana::test::ct_eq<3>{}, hana::test::ct_eq<4>{}),
            hana::test::ct_eq<4>{}
        ));
    }

    // eval_if
    {
        auto t = [](auto) { return hana::test::ct_eq<2>{}; };
        auto e = [](auto) { return hana::test::ct_eq<3>{}; };

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::eval_if(true_, t, invalid{}),
            hana::test::ct_eq<2>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::eval_if(false_, invalid{}, e),
            hana::test::ct_eq<3>{}
        ));
    }

    // while_
    {
        hana::test::_injection<0> f{};

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::while_(hana::not_equal.to(hana::test::ct_eq<0>{}), hana::test::ct_eq<0>{}, invalid{}),
            hana::test::ct_eq<0>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::while_(hana::not_equal.to(f(hana::test::ct_eq<0>{})), hana::test::ct_eq<0>{}, f),
            f(hana::test::ct_eq<0>{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::while_(hana::not_equal.to(f(f(hana::test::ct_eq<0>{}))), hana::test::ct_eq<0>{}, f),
            f(f(hana::test::ct_eq<0>{}))
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::while_(hana::not_equal.to(f(f(f(hana::test::ct_eq<0>{})))), hana::test::ct_eq<0>{}, f),
            f(f(f(hana::test::ct_eq<0>{})))
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::while_(hana::not_equal.to(f(f(f(f(hana::test::ct_eq<0>{}))))), hana::test::ct_eq<0>{}, f),
            f(f(f(f(hana::test::ct_eq<0>{}))))
        ));

        // Make sure it can be called with an lvalue state:
        auto state = hana::test::ct_eq<0>{};
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::while_(hana::not_equal.to(f(f(f(f(hana::test::ct_eq<0>{}))))), state, f),
            f(f(f(f(hana::test::ct_eq<0>{}))))
        ));
    }
}
