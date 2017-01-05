// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana.hpp>

#include <boost/hana/lazy.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/concept/comparable.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/functional/compose.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/applicative.hpp>
#include <laws/base.hpp>
#include <laws/comonad.hpp>
#include <laws/functor.hpp>
#include <laws/monad.hpp>

#include <array>
#include <iostream>
using namespace boost::hana;


namespace boost { namespace hana {
    // We provide this instance for unit tests only because it is _so_ much
    // more convenient, but this instance is too dangerous for general usage.
    // See the documentation of `hana::lazy` for more info.
    template <>
    struct equal_impl<lazy_tag, lazy_tag> {
        template <typename X, typename Y>
        static constexpr auto apply(X x, Y y)
        { return equal(eval(x), eval(y)); }
    };
}}

auto invalid = [](auto x)
{ return x.this_function_must_not_be_instantiated; };


int main() {
    test::_injection<0> f{};
    using test::ct_eq;

    auto eqs = make<tuple_tag>(make_lazy(ct_eq<0>{}), make_lazy(ct_eq<1>{}), make_lazy(ct_eq<2>{}));
    auto eq_elems = make<tuple_tag>(ct_eq<0>{}, ct_eq<1>{}, ct_eq<1>{});
    auto nested = make<tuple_tag>(
        make_lazy(make_lazy(ct_eq<0>{})),
        make_lazy(make_lazy(ct_eq<1>{})),
        make_lazy(make_lazy(ct_eq<2>{}))
    );

    //////////////////////////////////////////////////////////////////////////
    // Lazy methods
    //////////////////////////////////////////////////////////////////////////
    {
        // lazy
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                make_lazy(f)(),
                make_lazy(f())
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                make_lazy(f)(ct_eq<0>{}),
                make_lazy(f(ct_eq<0>{}))
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                make_lazy(f)(ct_eq<0>{}, ct_eq<1>{}),
                make_lazy(f(ct_eq<0>{}, ct_eq<1>{}))
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                make_lazy(f)(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
                make_lazy(f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}))
            ));

            // The function is not applied.
            make_lazy(invalid)();
            make_lazy(invalid)(ct_eq<0>{});
            make_lazy(invalid)(ct_eq<0>{}, ct_eq<1>{});
            make_lazy(invalid)(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{});
        }

        // eval
        {
            // With lazy expressions
            BOOST_HANA_CONSTANT_CHECK(equal(
                eval(make_lazy(ct_eq<0>{})),
                ct_eq<0>{}
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                eval(make_lazy(ct_eq<1>{})),
                ct_eq<1>{}
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                eval(make_lazy(f)()),
                f()
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                eval(make_lazy(f)(ct_eq<3>{})),
                f(ct_eq<3>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                eval(make_lazy(f)(ct_eq<3>{}, ct_eq<4>{})),
                f(ct_eq<3>{}, ct_eq<4>{})
            ));

            // Should call a nullary function
            BOOST_HANA_CONSTANT_CHECK(equal(
                eval([]{ return ct_eq<3>{}; }),
                ct_eq<3>{}
            ));

            // Should call a unary function with hana::id.
            BOOST_HANA_CONSTANT_CHECK(equal(
                eval([](auto _) { return _(ct_eq<3>{}); }),
                ct_eq<3>{}
            ));

            // For overloaded function objects that are both nullary and unary,
            // the nullary overload should be preferred.
            BOOST_HANA_CONSTANT_CHECK(equal(
                eval(f),
                f()
            ));
        }

        // Make sure this does not move from a destroyed object, as that
        // used to be the case.
        {
            auto x = flatten(make_lazy(make_lazy(test::Tracked{1})));
            auto z = eval(x); (void)z;
        }

        // In some cases where a type has a constructor that is way too
        // general, copying a lazy value holding an object of that type
        // could trigger the instantiation of that constructor. If that
        // constructor was ill-formed, the compilation would fail. We
        // make sure this does not happen.
        {
            {
                auto expr = make_lazy(test::trap_construct{});
                auto implicit_copy = expr;          (void)implicit_copy;
                decltype(expr) explicit_copy(expr); (void)explicit_copy;
            }

            {
                auto expr = make_lazy(test::trap_construct{})();
                auto implicit_copy = expr;          (void)implicit_copy;
                decltype(expr) explicit_copy(expr); (void)explicit_copy;
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Functor
    //////////////////////////////////////////////////////////////////////////
    {
        // transform
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                transform(make_lazy(ct_eq<0>{}), f),
                make_lazy(f(ct_eq<0>{}))
            ));
        }

        // laws
        test::TestFunctor<lazy_tag>{eqs, eq_elems};
    }

    //////////////////////////////////////////////////////////////////////////
    // Applicative
    //////////////////////////////////////////////////////////////////////////
    {
        // ap
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                ap(make_lazy(f), make_lazy(ct_eq<0>{})),
                make_lazy(f(ct_eq<0>{}))
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                ap(make_lazy(f), make_lazy(ct_eq<0>{}), make_lazy(ct_eq<1>{})),
                make_lazy(f(ct_eq<0>{}, ct_eq<1>{}))
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                ap(make_lazy(f), make_lazy(ct_eq<0>{}), make_lazy(ct_eq<1>{}), make_lazy(ct_eq<2>{})),
                make_lazy(f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}))
            ));

            // The function is not applied.
            ap(make_lazy(invalid), make_lazy(ct_eq<0>{}));
            ap(make_lazy(invalid), make_lazy(ct_eq<0>{}), make_lazy(ct_eq<1>{}));
            ap(make_lazy(invalid), make_lazy(ct_eq<0>{}), make_lazy(ct_eq<1>{}), make_lazy(ct_eq<2>{}));
        }

        // lift
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                lift<lazy_tag>(ct_eq<0>{}),
                make_lazy(ct_eq<0>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                lift<lazy_tag>(ct_eq<1>{}),
                make_lazy(ct_eq<1>{})
            ));
        }

        // laws
        test::TestApplicative<lazy_tag>{eqs};
    }

    //////////////////////////////////////////////////////////////////////////
    // Monad
    //////////////////////////////////////////////////////////////////////////
    {
        auto f_ = compose(make_lazy, f);

        // chain
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                chain(make_lazy(ct_eq<0>{}), f_),
                f_(ct_eq<0>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                chain(make_lazy(ct_eq<1>{}), f_),
                f_(ct_eq<1>{})
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                make_lazy(ct_eq<1>{}) | f_,
                f_(ct_eq<1>{})
            ));
        }

        // flatten
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                flatten(make_lazy(make_lazy(ct_eq<0>{}))),
                make_lazy(ct_eq<0>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                flatten(make_lazy(make_lazy(ct_eq<1>{}))),
                make_lazy(ct_eq<1>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                flatten(make_lazy(make_lazy(make_lazy(ct_eq<1>{})))),
                make_lazy(make_lazy(ct_eq<1>{}))
            ));
        }

        // laws
        test::TestMonad<lazy_tag>{eqs, nested};
    }

    //////////////////////////////////////////////////////////////////////////
    // Comonad
    //////////////////////////////////////////////////////////////////////////
    {
        // extract
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                extract(make_lazy(ct_eq<4>{})),
                ct_eq<4>{}
            ));
        }

        // duplicate
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                duplicate(make_lazy(ct_eq<4>{})),
                make_lazy(make_lazy(ct_eq<4>{}))
            ));
        }

        // extend
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                extend(make_lazy(ct_eq<4>{}), f),
                make_lazy(f(make_lazy(ct_eq<4>{})))
            ));
        }

        // laws
        test::TestComonad<lazy_tag>{eqs};
    }

    //////////////////////////////////////////////////////////////////////////
    // Make sure the monadic chain is evaluated in the right order.
    //////////////////////////////////////////////////////////////////////////
    {
        std::array<bool, 3> executed = {{false, false, false}};
        int dummy = 0;

        std::cout << "creating the monadic chain...\n";
        auto chain = make_lazy(dummy)
            | [&](int dummy) {
                std::cout << "executing the first computation...\n";
                executed[0] = true;
                BOOST_HANA_RUNTIME_CHECK(
                    executed == std::array<bool, 3>{{true, false, false}}
                );
                return make_lazy(dummy);
            }
            | [&](int dummy) {
                std::cout << "executing the second computation...\n";
                executed[1] = true;
                BOOST_HANA_RUNTIME_CHECK(
                    executed == std::array<bool, 3>{{true, true, false}}
                );
                return make_lazy(dummy);
            }
            | [&](int dummy) {
                std::cout << "executing the third computation...\n";
                executed[2] = true;
                BOOST_HANA_RUNTIME_CHECK(
                    executed == std::array<bool, 3>{{true, true, true}}
                );
                return make_lazy(dummy);
            };

        BOOST_HANA_RUNTIME_CHECK(
            executed == std::array<bool, 3>{{false, false, false}}
        );

        std::cout << "evaluating the chain...\n";
        eval(chain);
    }
}
