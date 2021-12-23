// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/adjust_if.hpp>
#include <boost/hana/ap.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/chain.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/fill.hpp>
#include <boost/hana/functional/always.hpp>
#include <boost/hana/functional/compose.hpp>
#include <boost/hana/lift.hpp>
#include <boost/hana/replace_if.hpp>
#include <boost/hana/tap.hpp>
#include <boost/hana/then.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/applicative.hpp>
#include <laws/base.hpp>
#include <laws/functor.hpp>
#include <laws/monad.hpp>
#include <support/cnumeric.hpp>
#include <support/identity.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    hana::test::_injection<0> f{};

    // Functor
    {
        auto functor = ::identity;
        // adjust_if
        {
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::adjust_if(functor(ct_eq<0>{}), hana::always(cnumeric<bool, true>), f),
                functor(f(ct_eq<0>{}))
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::adjust_if(functor(ct_eq<0>{}), hana::always(cnumeric<bool, false>), f),
                functor(ct_eq<0>{})
            ));
        }

        // fill
        {
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::fill(functor(ct_eq<0>{}), ct_eq<1>{}),
                functor(ct_eq<1>{})
            ));
        }

        // transform
        {
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::transform(functor(ct_eq<0>{}), f),
                functor(f(ct_eq<0>{}))
            ));
        }

        // replace_if
        {
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace_if(functor(ct_eq<0>{}), hana::always(cnumeric<bool, true>), ct_eq<1>{}),
                functor(ct_eq<1>{})
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace_if(functor(ct_eq<0>{}), hana::always(cnumeric<bool, false>), ct_eq<1>{}),
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
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::ap(a(f), a(ct_eq<0>{})),
                a(f(ct_eq<0>{}))
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::ap(a(f), a(ct_eq<0>{}), a(ct_eq<1>{})),
                a(f(ct_eq<0>{}, ct_eq<1>{}))
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::ap(a(f), a(ct_eq<0>{}), a(ct_eq<1>{}), a(ct_eq<2>{})),
                a(f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}))
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::ap(a(f), a(ct_eq<0>{}), a(ct_eq<1>{}), a(ct_eq<2>{}), a(ct_eq<3>{})),
                a(f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}))
            ));
        }

        // lift
        {
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::lift<A>(ct_eq<0>{}),
                a(ct_eq<0>{})
            ));
        }
    }

    // Monad
    {
        auto monad = ::identity;
        using M = ::Identity;
        auto f = hana::compose(monad, hana::test::_injection<0>{});

        // chain
        {
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::chain(monad(ct_eq<1>{}), f),
                f(ct_eq<1>{})
            ));
        }

        // tap
        {
            bool executed = false;
            auto exec = [&](auto) { executed = true; };
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::chain(monad(ct_eq<0>{}), hana::tap<M>(exec)),
                monad(ct_eq<0>{})
            ));
            BOOST_HANA_RUNTIME_CHECK(executed);
        }

        // then
        {
            struct invalid { };
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::then(monad(invalid{}), monad(ct_eq<0>{})),
                monad(ct_eq<0>{})
            ));
        }
    }
}
