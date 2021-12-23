// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_LAWS_COMPARABLE_HPP
#define BOOST_HANA_TEST_LAWS_COMPARABLE_HPP

#include <boost/hana/and.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/comparing.hpp>
#include <boost/hana/concept/comparable.hpp>
#include <boost/hana/concept/constant.hpp>
#include <boost/hana/concept/product.hpp>
#include <boost/hana/concept/sequence.hpp>
#include <boost/hana/core/make.hpp>
#include <boost/hana/core/when.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/first.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/lazy.hpp>
#include <boost/hana/not_equal.hpp>
#include <boost/hana/second.hpp>
#include <boost/hana/value.hpp>


#include <laws/base.hpp>


namespace boost { namespace hana { namespace test {
    template <typename T, typename = hana::when<true>>
    struct TestComparable : TestComparable<T, laws> {
        using TestComparable<T, laws>::TestComparable;
    };

    template <typename T>
    struct TestComparable<T, laws> {
        template <typename Xs>
        TestComparable(Xs xs) {
            hana::for_each(xs, [](auto x) {
                static_assert(hana::Comparable<decltype(x)>{}, "");
            });

            foreach2(xs, [](auto a, auto b) {

                // reflexivity
                BOOST_HANA_CHECK(
                    hana::equal(a, a)
                );

                // symmetry
                BOOST_HANA_CHECK(
                    hana::equal(a, b) ^implies^ hana::equal(b, a)
                );

                // `not_equal` is the negation of `equal`
                BOOST_HANA_CHECK(
                    hana::not_equal(a, b) ^iff^ hana::not_(hana::equal(a, b))
                );

                // equal.to and not_equal.to
                BOOST_HANA_CHECK(
                    hana::equal.to(a)(b) ^iff^ hana::equal(a, b)
                );

                BOOST_HANA_CHECK(
                    hana::not_equal.to(a)(b) ^iff^ hana::not_equal(a, b)
                );

                // comparing
                _injection<0> f{};
                BOOST_HANA_CHECK(
                    hana::comparing(f)(a, b) ^iff^ hana::equal(f(a), f(b))
                );
            });

            // transitivity
            foreach3(xs, [](auto a, auto b, auto c) {
                BOOST_HANA_CHECK(
                    hana::and_(hana::equal(a, b), hana::equal(b, c))
                        ^implies^ hana::equal(a, c)
                );
            });
        }
    };

    template <typename C>
    struct TestComparable<C, when<Constant<C>::value>>
        : TestComparable<C, laws>
    {
        template <typename Xs>
        TestComparable(Xs xs) : TestComparable<C, laws>{xs} {
            foreach2(xs, [](auto a, auto b) {
                BOOST_HANA_CHECK(
                    hana::value(hana::equal(a, b)) ^iff^
                        hana::equal(hana::value(a), hana::value(b))
                );
            });
        }
    };

    template <typename P>
    struct TestComparable<P, when<Product<P>::value>>
        : TestComparable<P, laws>
    {
        template <typename Products>
        TestComparable(Products products) : TestComparable<P, laws>{products} {
            foreach2(products, [](auto x, auto y) {
                BOOST_HANA_CHECK(
                    hana::equal(x, y) ^iff^
                    hana::and_(
                        hana::equal(hana::first(x), hana::first(y)),
                        hana::equal(hana::second(x), hana::second(y))
                    )
                );
            });
        }
    };

    template <typename S>
    struct TestComparable<S, when<Sequence<S>::value>>
        : TestComparable<S, laws>
    {
        template <int i>
        using x = _constant<i>;

        template <typename Xs>
        TestComparable(Xs xs) : TestComparable<S, laws>{xs} {
            constexpr auto list = make<S>;

            //////////////////////////////////////////////////////////////////
            // equal
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                list(),
                list()
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
                list(x<0>{}),
                list()
            )));
            BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
                list(),
                list(x<0>{})
            )));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                list(x<0>{}),
                list(x<0>{})
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
                list(x<0>{}, x<1>{}),
                list(x<0>{})
            )));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                list(x<0>{}, x<1>{}),
                list(x<0>{}, x<1>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
                list(x<0>{}, x<1>{}, x<2>{}, x<3>{}),
                list(x<0>{}, x<1>{}, x<2>{}, x<4>{})
            )));
        }
    };
}}} // end namespace boost::hana::test

#endif // !BOOST_HANA_TEST_LAWS_COMPARABLE_HPP
