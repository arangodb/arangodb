// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/functional/always.hpp>
#include <boost/hana/functional/compose.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/applicative.hpp>
#include <laws/base.hpp>
#include <laws/functor.hpp>
#include <laws/monad.hpp>
#include <support/cnumeric.hpp>
#include <support/identity.hpp>
using namespace boost::hana;


int main() {
    using test::ct_eq;
    test::_injection<0> f{};

    // Functor
    {
        auto functor = ::identity;
        // adjust_if
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                adjust_if(functor(ct_eq<0>{}), always(cnumeric<bool, true>), f),
                functor(f(ct_eq<0>{}))
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                adjust_if(functor(ct_eq<0>{}), always(cnumeric<bool, false>), f),
                functor(ct_eq<0>{})
            ));
        }

        // fill
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                fill(functor(ct_eq<0>{}), ct_eq<1>{}),
                functor(ct_eq<1>{})
            ));
        }

        // transform
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                transform(functor(ct_eq<0>{}), f),
                functor(f(ct_eq<0>{}))
            ));
        }

        // replace_if
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                replace_if(functor(ct_eq<0>{}), always(cnumeric<bool, true>), ct_eq<1>{}),
                functor(ct_eq<1>{})
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                replace_if(functor(ct_eq<0>{}), always(cnumeric<bool, false>), ct_eq<1>{}),
                functor(ct_eq<0>{})
            ));
        }
    }

    // Applicative
    {
        auto a = ::identity;
        using A = ::Identity;

        // ap
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                ap(a(f), a(ct_eq<0>{})),
                a(f(ct_eq<0>{}))
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                ap(a(f), a(ct_eq<0>{}), a(ct_eq<1>{})),
                a(f(ct_eq<0>{}, ct_eq<1>{}))
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                ap(a(f), a(ct_eq<0>{}), a(ct_eq<1>{}), a(ct_eq<2>{})),
                a(f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}))
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                ap(a(f), a(ct_eq<0>{}), a(ct_eq<1>{}), a(ct_eq<2>{}), a(ct_eq<3>{})),
                a(f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}))
            ));
        }

        // lift
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                lift<A>(ct_eq<0>{}),
                a(ct_eq<0>{})
            ));
        }
    }

    // Monad
    {
        auto monad = ::identity;
        using M = ::Identity;
        auto f = compose(monad, test::_injection<0>{});

        // chain
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                chain(monad(ct_eq<1>{}), f),
                f(ct_eq<1>{})
            ));
        }

        // tap
        {
            bool executed = false;
            auto exec = [&](auto) { executed = true; };
            BOOST_HANA_CONSTANT_CHECK(equal(
                chain(monad(ct_eq<0>{}), tap<M>(exec)),
                monad(ct_eq<0>{})
            ));
            BOOST_HANA_RUNTIME_CHECK(executed);
        }

        // then
        {
            struct invalid { };
            BOOST_HANA_CONSTANT_CHECK(equal(
                then(monad(invalid{}), monad(ct_eq<0>{})),
                monad(ct_eq<0>{})
            ));
        }
    }
}
