// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_LAWS_FUNCTOR_HPP
#define BOOST_HANA_TEST_LAWS_FUNCTOR_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/concept/comparable.hpp>
#include <boost/hana/core/when.hpp>
#include <boost/hana/functional/capture.hpp>
#include <boost/hana/concept/functor.hpp>
#include <boost/hana/lazy.hpp>

#include <laws/base.hpp>


namespace boost { namespace hana { namespace test {
    template <typename F, typename = when<true>>
    struct TestFunctor : TestFunctor<F, laws> {
        using TestFunctor<F, laws>::TestFunctor;
    };

    template <typename F>
    struct TestFunctor<F, laws> {
        template <typename Xs, typename Elements>
        TestFunctor(Xs xs, Elements elements) {
            hana::for_each(xs, hana::capture(elements)([](auto elements, auto x) {
                static_assert(Functor<decltype(x)>::value, "");

                test::_injection<0> f{};
                test::_injection<1> g{};

                // identity
                BOOST_HANA_CHECK(hana::equal(
                    hana::transform(x, id),
                    x
                ));

                // composition
                BOOST_HANA_CHECK(hana::equal(
                    hana::transform(x, hana::compose(f, g)),
                    hana::transform(hana::transform(x, g), f)
                ));

                // method definitions in terms of transform/adjust_if
                hana::for_each(elements, hana::capture(x, f, elements)(
                [](auto x, auto f, auto elements, auto value) {
                    BOOST_HANA_CHECK(hana::equal(
                        hana::adjust(x, value, f),
                        hana::adjust_if(x, hana::equal.to(value), f)
                    ));

                    hana::for_each(elements, hana::capture(x, value)(
                    [](auto x, auto oldval, auto newval) {
                        BOOST_HANA_CHECK(hana::equal(
                            hana::replace(x, oldval, newval),
                            hana::replace_if(x, hana::equal.to(oldval), newval)
                        ));
                    }));
                }));

                auto pred = hana::always(true_c);
                BOOST_HANA_CHECK(hana::equal(
                    hana::adjust_if(x, pred, f),
                    hana::transform(x, [=](auto z) {
                        return hana::eval_if(pred(z),
                            hana::make_lazy(f)(z),
                            hana::make_lazy(z)
                        );
                    })
                ));

                test::_constant<0> v{};
                BOOST_HANA_CHECK(hana::equal(
                    hana::replace_if(x, pred, v),
                    hana::adjust_if(x, pred, hana::always(v))
                ));

                BOOST_HANA_CHECK(hana::equal(
                    hana::fill(x, v),
                    hana::replace_if(x, hana::always(true_c), v)
                ));

            }));
        }
    };

    template <typename S>
    struct TestFunctor<S, when<Sequence<S>::value>>
        : TestFunctor<S, laws>
    {
        struct undefined { };

        template <typename Xs, typename Elements>
        TestFunctor(Xs xs, Elements elements)
            : TestFunctor<S, laws>{xs, elements}
        {
            using test::ct_eq;
            using test::cx_eq;
            constexpr auto list = make<S>;

            //////////////////////////////////////////////////////////////////
            // replace_if
            //////////////////////////////////////////////////////////////////
            {
            auto a = ct_eq<888>{};
            auto b = ct_eq<999>{};

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace_if(list(), undefined{}, undefined{}),
                list()
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace_if(list(ct_eq<0>{}), equal.to(a), undefined{}),
                list(ct_eq<0>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace_if(list(a), equal.to(a), b),
                list(b)
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace_if(list(ct_eq<0>{}, ct_eq<1>{}), equal.to(a), undefined{}),
                list(ct_eq<0>{}, ct_eq<1>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace_if(list(a, ct_eq<1>{}), equal.to(a), b),
                list(b, ct_eq<1>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace_if(list(ct_eq<0>{}, a), equal.to(a), b),
                list(ct_eq<0>{}, b)
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace_if(list(a, a), equal.to(a), b),
                list(b, b)
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace_if(list(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), equal.to(a), undefined{}),
                list(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace_if(list(a, ct_eq<1>{}, ct_eq<2>{}), equal.to(a), b),
                list(b, ct_eq<1>{}, ct_eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace_if(list(ct_eq<0>{}, a, ct_eq<2>{}), equal.to(a), b),
                list(ct_eq<0>{}, b, ct_eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace_if(list(ct_eq<0>{}, ct_eq<1>{}, a), equal.to(a), b),
                list(ct_eq<0>{}, ct_eq<1>{}, b)
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace_if(list(a, ct_eq<1>{}, a), equal.to(a), b),
                list(b, ct_eq<1>{}, b)
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace_if(list(a, ct_eq<1>{}, a, ct_eq<3>{}, a), equal.to(a), b),
                list(b, ct_eq<1>{}, b, ct_eq<3>{}, b)
            ));
            }

            //////////////////////////////////////////////////////////////////
            // replace
            //////////////////////////////////////////////////////////////////
            {
            auto a = ct_eq<888>{};
            auto b = ct_eq<999>{};

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace(list(), undefined{}, undefined{}),
                list()
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace(list(ct_eq<0>{}), a, undefined{}),
                list(ct_eq<0>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace(list(a), a, b),
                list(b)
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace(list(ct_eq<0>{}, ct_eq<1>{}), a, undefined{}),
                list(ct_eq<0>{}, ct_eq<1>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace(list(a, ct_eq<1>{}), a, b),
                list(b, ct_eq<1>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace(list(ct_eq<0>{}, a), a, b),
                list(ct_eq<0>{}, b)
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace(list(a, a), a, b),
                list(b, b)
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace(list(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), a, undefined{}),
                list(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace(list(a, ct_eq<1>{}, ct_eq<2>{}), a, b),
                list(b, ct_eq<1>{}, ct_eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace(list(ct_eq<0>{}, a, ct_eq<2>{}), a, b),
                list(ct_eq<0>{}, b, ct_eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace(list(ct_eq<0>{}, ct_eq<1>{}, a), a, b),
                list(ct_eq<0>{}, ct_eq<1>{}, b)
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace(list(a, ct_eq<1>{}, a), a, b),
                list(b, ct_eq<1>{}, b)
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::replace(list(a, ct_eq<1>{}, a, ct_eq<3>{}, a), a, b),
                list(b, ct_eq<1>{}, b, ct_eq<3>{}, b)
            ));
            }
        }
    };
}}} // end namespace boost::hana::test

#endif // !BOOST_HANA_TEST_LAWS_FUNCTOR_HPP
