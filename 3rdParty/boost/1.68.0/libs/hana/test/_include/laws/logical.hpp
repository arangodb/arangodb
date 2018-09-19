// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_LAWS_LOGICAL_HPP
#define BOOST_HANA_TEST_LAWS_LOGICAL_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/concept/comparable.hpp>
#include <boost/hana/core/when.hpp>
#include <boost/hana/functional/capture.hpp>
#include <boost/hana/lazy.hpp>
#include <boost/hana/concept/logical.hpp>

#include <laws/base.hpp>


namespace boost { namespace hana { namespace test {
    template <typename L, typename = when<true>>
    struct TestLogical : TestLogical<L, laws> {
        using TestLogical<L, laws>::TestLogical;
    };

    template <typename L>
    struct TestLogical<L, laws> {
        template <typename Xs, typename Pred, typename F>
        static void for_each_such_that(Xs xs, Pred pred, F f) {
            hana::for_each(xs, [&pred, &f](auto x) {
                hana::eval_if(pred(x),
                    hana::make_lazy(f)(x),
                    [](auto) { }
                );
            });
        }

        template <typename Xs>
        TestLogical(Xs xs) {
            hana::for_each(xs, [](auto x) {
                static_assert(Logical<decltype(x)>{}, "");
            });

            foreach3(xs, hana::capture(xs)([](auto xs, auto a, auto b, auto c) {
                auto true_valued = [](auto x) {
                    return hana::if_(x, true_c, false_c);
                };
                auto false_valued = [](auto x) {
                    return hana::if_(x, false_c, true_c);
                };

                // associativity
                BOOST_HANA_CHECK(hana::equal(
                    hana::or_(a, hana::or_(b, c)),
                    hana::or_(hana::or_(a, b), c)
                ));
                BOOST_HANA_CHECK(hana::equal(
                    hana::and_(a, hana::and_(b, c)),
                    hana::and_(hana::and_(a, b), c)
                ));

                // equivalence through commutativity
                BOOST_HANA_CHECK(
                    hana::or_(a, b) ^iff^ hana::or_(b, a)
                );
                BOOST_HANA_CHECK(
                    hana::and_(a, b) ^iff^ hana::and_(b, a)
                );

                // absorption
                BOOST_HANA_CHECK(hana::equal(
                    hana::or_(a, hana::and_(a, b)), a
                ));
                BOOST_HANA_CHECK(hana::equal(
                    hana::and_(a, hana::or_(a, b)), a
                ));

                // left identity
                TestLogical::for_each_such_that(xs, true_valued,
                hana::capture(a)([](auto a, auto t) {
                    BOOST_HANA_CHECK(hana::equal(
                        hana::and_(t, a), a
                    ));
                }));
                TestLogical::for_each_such_that(xs, false_valued,
                hana::capture(a)([](auto a, auto f) {
                    BOOST_HANA_CHECK(hana::equal(
                        hana::or_(f, a), a
                    ));
                }));

                // distributivity
                BOOST_HANA_CHECK(hana::equal(
                    hana::or_(a, hana::and_(b, c)),
                    hana::and_(hana::or_(a, b), hana::or_(a, c))
                ));
                BOOST_HANA_CHECK(hana::equal(
                    hana::and_(a, hana::or_(b, c)),
                    hana::or_(hana::and_(a, b), hana::and_(a, c))
                ));

                // complements
                BOOST_HANA_CHECK(true_valued(hana::or_(a, hana::not_(a))));
                BOOST_HANA_CHECK(false_valued(hana::and_(a, hana::not_(a))));

            }));
        }
    };

    template <typename C>
    struct TestLogical<C, when<Constant<C>::value>>
        : TestLogical<C, laws>
    {
        template <typename Xs>
        TestLogical(Xs xs) : TestLogical<C, laws>{xs} {
            foreach2(xs, [](auto x, auto y) {

                BOOST_HANA_CHECK(hana::equal(
                    hana::value(hana::not_(x)),
                    hana::not_(hana::value(x))
                ));

                BOOST_HANA_CHECK(hana::equal(
                    hana::value(hana::and_(x, y)),
                    hana::and_(hana::value(x), hana::value(y))
                ));

                BOOST_HANA_CHECK(hana::equal(
                    hana::value(hana::or_(x, y)),
                    hana::or_(hana::value(x), hana::value(y))
                ));

            });
        }
    };
}}} // end namespace boost::hana::test

#endif // !BOOST_HANA_TEST_LAWS_LOGICAL_HPP
