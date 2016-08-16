// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_LAWS_APPLICATIVE_HPP
#define BOOST_HANA_TEST_LAWS_APPLICATIVE_HPP

#include <boost/hana/concept/applicative.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/concept/comparable.hpp>
#include <boost/hana/core/when.hpp>
#include <boost/hana/functional/capture.hpp>
#include <boost/hana/functional/compose.hpp>
#include <boost/hana/functional/curry.hpp>
#include <boost/hana/functional/id.hpp>
#include <boost/hana/functional/placeholder.hpp>

#include <laws/base.hpp>


namespace boost { namespace hana { namespace test {
    template <typename F, typename = when<true>>
    struct TestApplicative : TestApplicative<F, laws> {
        using TestApplicative<F, laws>::TestApplicative;
    };

    template <typename F>
    struct TestApplicative<F, laws> {
        template <typename Applicatives>
        TestApplicative(Applicatives applicatives) {
            hana::for_each(applicatives, [](auto a) {
                static_assert(Applicative<decltype(a)>::value, "");
            });

            auto functions1 = hana::take_front(
            hana::transform(applicatives, [](auto xs) {
                return hana::transform(xs, hana::curry<2>(test::_injection<0>{}));
            }), hana::int_c<3>);

            auto functions2 = hana::take_front(
            hana::transform(applicatives, [](auto xs) {
                return hana::transform(xs, hana::curry<2>(test::_injection<1>{}));
            }), hana::int_c<3>);

            // identity
            {
                hana::for_each(applicatives, [](auto xs) {
                    BOOST_HANA_CHECK(hana::equal(
                        hana::ap(hana::lift<F>(hana::id), xs),
                        xs
                    ));
                });
            }

            // composition
            {
                hana::for_each(applicatives, hana::capture(functions1, functions2)(
                [](auto functions1, auto functions2, auto xs) {
                hana::for_each(functions1, hana::capture(functions2, xs)(
                [](auto functions2, auto xs, auto fs) {
                hana::for_each(functions2, hana::capture(xs, fs)(
                [](auto xs, auto fs, auto gs) {
                    BOOST_HANA_CHECK(hana::equal(
                        hana::ap(hana::ap(hana::lift<F>(compose), fs, gs), xs),
                        hana::ap(fs, hana::ap(gs, xs))
                    ));
                }));}));}));
            }

            // homomorphism
            {
                test::_injection<0> f{};
                test::ct_eq<3> x{};
                BOOST_HANA_CONSTANT_CHECK(hana::equal(
                    hana::ap(hana::lift<F>(f), hana::lift<F>(x)),
                    hana::lift<F>(f(x))
                ));
            }

            // interchange
            {
                hana::for_each(functions1, [](auto fs) {
                    test::ct_eq<4> x{};
                    BOOST_HANA_CHECK(hana::equal(
                        hana::ap(fs, hana::lift<F>(x)),
                        hana::ap(hana::lift<F>(hana::_(x)), fs)
                    ));
                });
            }

            // definition of transform
            {
                hana::for_each(applicatives, [](auto xs) {
                    test::_injection<0> f{};
                    BOOST_HANA_CHECK(hana::equal(
                        hana::transform(xs, f),
                        hana::ap(hana::lift<F>(f), xs)
                    ));
                });
            }
        }
    };

    template <typename S>
    struct TestApplicative<S, when<Sequence<S>::value>>
        : TestApplicative<S, laws>
    {
        template <typename Applicatives>
        TestApplicative(Applicatives applicatives)
            : TestApplicative<S, laws>{applicatives}
        {
            _injection<0> f{};
            _injection<1> g{};
            using test::ct_eq;
            constexpr auto list = make<S>;

            //////////////////////////////////////////////////////////////////
            // ap
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::ap(list(), list()),
                list()
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::ap(list(), list(ct_eq<0>{})),
                list()
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::ap(list(), list(ct_eq<0>{}, ct_eq<1>{})),
                list()
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::ap(list(), list(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
                list()
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::ap(list(f), list()),
                list()
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::ap(list(f), list(ct_eq<0>{})),
                list(f(ct_eq<0>{}))
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::ap(list(f), list(ct_eq<0>{}, ct_eq<1>{})),
                list(f(ct_eq<0>{}), f(ct_eq<1>{}))
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::ap(list(f), list(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
                list(f(ct_eq<0>{}), f(ct_eq<1>{}), f(ct_eq<2>{}))
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::ap(list(f, g), list()),
                list()
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::ap(list(f, g), list(ct_eq<0>{})),
                list(f(ct_eq<0>{}), g(ct_eq<0>{}))
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::ap(list(f, g), list(ct_eq<0>{}, ct_eq<1>{})),
                list(f(ct_eq<0>{}), f(ct_eq<1>{}), g(ct_eq<0>{}), g(ct_eq<1>{}))
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::ap(list(f, g), list(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
                list(f(ct_eq<0>{}), f(ct_eq<1>{}), f(ct_eq<2>{}),
                     g(ct_eq<0>{}), g(ct_eq<1>{}), g(ct_eq<2>{}))
            ));

            //////////////////////////////////////////////////////////////////
            // lift
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                lift<S>(ct_eq<0>{}),
                list(ct_eq<0>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                lift<S>(ct_eq<1>{}),
                list(ct_eq<1>{})
            ));
        }
    };
}}} // end namespace boost::hana::test

#endif // !BOOST_HANA_TEST_LAWS_APPLICATIVE_HPP
