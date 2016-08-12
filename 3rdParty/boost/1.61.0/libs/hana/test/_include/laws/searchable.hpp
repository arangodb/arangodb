// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_LAWS_SEARCHABLE_HPP
#define BOOST_HANA_TEST_LAWS_SEARCHABLE_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/concept/comparable.hpp>
#include <boost/hana/core/is_a.hpp>
#include <boost/hana/core/when.hpp>
#include <boost/hana/functional/capture.hpp>
#include <boost/hana/lazy.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/concept/searchable.hpp>

#include <laws/base.hpp>
#include <support/numeric.hpp>


namespace boost { namespace hana { namespace test {
    template <typename S, typename = when<true>>
    struct TestSearchable : TestSearchable<S, laws> {
        using TestSearchable<S, laws>::TestSearchable;
    };

    template <typename S>
    struct TestSearchable<S, laws> {
        template <typename Searchables, typename Keys>
        TestSearchable(Searchables searchables, Keys keys) {
            hana::for_each(searchables, [](auto xs) {
                static_assert(Searchable<decltype(xs)>::value, "");
            });

            auto predicates = hana::concat(
                hana::to_tuple(hana::transform(keys, equal.to)),
                hana::make_tuple(
                    hana::always(false_c),
                    hana::always(true_c)
                )
            );

            hana::for_each(searchables, hana::capture(searchables, keys, predicates)(
            [](auto searchables, auto keys, auto predicates, auto xs) {
                hana::for_each(predicates, hana::capture(xs)(
                [](auto xs, auto p) {
                    // any_of(xs, p) <=> !all_of(xs, negated p)
                    //               <=> !none_of(xs, p)
                    BOOST_HANA_CHECK(
                        hana::any_of(xs, p) ^iff^
                        hana::not_(hana::all_of(xs, hana::compose(not_, p)))
                    );

                    BOOST_HANA_CHECK(
                        hana::any_of(xs, p) ^iff^
                        hana::not_(hana::none_of(xs, p))
                    );
                }));

                // any(xs)  <=> any_of(xs, id)
                // all(xs)  <=> all_of(xs, id)
                // none(xs) <=> none_of(xs, id)
                auto all_logicals = hana::all_of(xs, [](auto x) {
                    return hana::bool_c<hana::Logical<decltype(x)>::value>;
                });
                only_when_(all_logicals, hana::make_lazy([](auto xs) {
                    BOOST_HANA_CHECK(
                        hana::any(xs) ^iff^ hana::any_of(xs, id)
                    );

                    BOOST_HANA_CHECK(
                        hana::all(xs) ^iff^ hana::all_of(xs, hana::id)
                    );

                    BOOST_HANA_CHECK(
                        hana::none(xs) ^iff^ hana::none_of(xs, hana::id)
                    );
                })(xs));

                // find_if(xs, always(false_c)) == nothing
                BOOST_HANA_CONSTANT_CHECK(hana::equal(
                    hana::find_if(xs, hana::always(hana::false_c)),
                    hana::nothing
                ));

                hana::for_each(searchables, hana::capture(xs)([](auto xs, auto ys) {
                    // is_subset(xs, ys) <=> all_of(xs, [](auto x) { return contains(ys, x); })
                    BOOST_HANA_CHECK(
                        hana::is_subset(xs, ys) ^iff^
                            hana::all_of(xs, hana::partial(hana::contains, ys))
                    );

                    // is_disjoint(xs, ys) <=> none_of(xs, [](auto x) { return contains(ys, x); })
                    BOOST_HANA_CHECK(
                        hana::is_disjoint(xs, ys) ^iff^
                            hana::none_of(xs, hana::partial(hana::contains, ys))
                    );
                }));

                hana::for_each(keys, hana::capture(xs)([](auto xs, auto key) {
                    // find(xs, x) == find_if(xs, [](auto y) { return y == x; })
                    BOOST_HANA_CHECK(hana::equal(
                        hana::find(xs, key),
                        hana::find_if(xs, hana::equal.to(key))
                    ));

                    // contains(xs, x) <=> any_of(xs, [](auto y) { return y == x; })
                    BOOST_HANA_CHECK(
                        hana::contains(xs, key) ^iff^
                        hana::any_of(xs, hana::equal.to(key))
                    );

                    only_when_(hana::contains(xs, key),
                        hana::make_lazy([](auto xs, auto key) {
                            // at_key(xs, key) == find(xs, key).value()
                            BOOST_HANA_CHECK(hana::equal(
                                hana::at_key(xs, key),
                                hana::find(xs, key).value()
                            ));
                        })(xs, key)
                    );
                }));
            }));
        }
    };

    template <typename S>
    struct TestSearchable<S, when<Sequence<S>::value>>
        : TestSearchable<S, laws>
    {
        template <int i>
        using x = _constant<i>;

        template <int = 0>
        struct invalid { };

        struct undefined { };

        template <typename Xs, typename Keys>
        TestSearchable(Xs xs, Keys keys)
            : TestSearchable<S, laws>{xs, keys}
        {
            constexpr auto list = make<S>;

            BOOST_HANA_CONSTEXPR_LAMBDA auto is_even = [](auto x) {
                return x % 2 == 0;
            };

            auto c = numeric; // a minimal comparable
            auto logical = numeric;

            //////////////////////////////////////////////////////////////////
            // any_of
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(
                hana::not_(hana::any_of(list(), equal.to(x<9>{})))
            );

            BOOST_HANA_CONSTANT_CHECK(
                hana::not_(hana::any_of(list(x<0>{}), equal.to(x<9>{})))
            );
            BOOST_HANA_CONSTANT_CHECK(
                hana::any_of(list(x<0>{}), equal.to(x<0>{}))
            );
            BOOST_HANA_CONSTANT_CHECK(
                hana::any_of(list(x<0>{}, invalid<1>{}), equal.to(x<0>{}))
            );
            BOOST_HANA_CONSTANT_CHECK(
                hana::any_of(list(x<0>{}, invalid<1>{}, invalid<2>{}), equal.to(x<0>{}))
            );

            BOOST_HANA_CONSTANT_CHECK(
                hana::not_(hana::any_of(list(x<0>{}, x<1>{}), equal.to(x<9>{})))
            );
            BOOST_HANA_CONSTANT_CHECK(
                hana::any_of(list(x<0>{}, x<1>{}), equal.to(x<1>{}))
            );
            BOOST_HANA_CONSTANT_CHECK(
                hana::any_of(list(x<0>{}, x<1>{}, invalid<2>{}), equal.to(x<1>{}))
            );
            BOOST_HANA_CONSTANT_CHECK(
                hana::any_of(list(x<0>{}, x<1>{}, invalid<2>{}, invalid<3>{}), equal.to(x<1>{}))
            );

            BOOST_HANA_CONSTANT_CHECK(
                hana::not_(hana::any_of(list(x<0>{}, x<1>{}, x<2>{}), equal.to(x<9>{})))
            );
            BOOST_HANA_CONSTANT_CHECK(
                hana::any_of(list(x<0>{}, x<1>{}, x<2>{}), equal.to(x<2>{}))
            );
            BOOST_HANA_CONSTANT_CHECK(
                hana::any_of(list(x<0>{}, x<1>{}, x<2>{}, nothing), equal.to(x<2>{}))
            );
            BOOST_HANA_CONSTANT_CHECK(
                hana::any_of(list(x<0>{}, x<1>{}, x<2>{}, nothing, nothing), equal.to(x<2>{}))
            );

            BOOST_HANA_CONSTANT_CHECK(
                hana::not_(hana::any_of(list(), invalid<>{}))
            );


            BOOST_HANA_CONSTEXPR_CHECK(
                hana::any_of(list(c(0)), equal.to(c(0)))
            );
            BOOST_HANA_CONSTEXPR_CHECK(
                hana::not_(hana::any_of(list(c(0)), equal.to(c(1))))
            );
            BOOST_HANA_CONSTEXPR_CHECK(
                hana::not_(hana::any_of(list(1), is_even))
            );
            BOOST_HANA_CONSTEXPR_CHECK(
                hana::any_of(list(2), is_even)
            );
            BOOST_HANA_CONSTEXPR_CHECK(
                hana::any_of(list(1, 2), is_even)
            );
            BOOST_HANA_CONSTEXPR_CHECK(
                hana::not_(hana::any_of(list(1, 3), is_even))
            );
            BOOST_HANA_CONSTEXPR_CHECK(
                hana::any_of(list(1, 3, 4), is_even)
            );

            //////////////////////////////////////////////////////////////////
            // any
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::any(list())));
            BOOST_HANA_CONSTEXPR_CHECK(hana::any(list(logical(true))));
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::any(list(logical(false)))));


            //////////////////////////////////////////////////////////////////
            // all_of
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(
                all_of(list(), invalid<>{})
            );
            BOOST_HANA_CONSTEXPR_CHECK(
                all_of(list(c(0)), equal.to(c(0)))
            );
            BOOST_HANA_CONSTEXPR_CHECK(
                not_(all_of(list(c(0)), equal.to(c(1))))
            );
            BOOST_HANA_CONSTEXPR_CHECK(not_(
                all_of(list(c(0), c(1)), equal.to(c(0)))
            ));
            BOOST_HANA_CONSTEXPR_CHECK(
                all_of(list(c(0), c(0)), equal.to(c(0)))
            );

            BOOST_HANA_CONSTEXPR_CHECK(not_(all_of(list(1), is_even)));
            BOOST_HANA_CONSTEXPR_CHECK(all_of(list(2), is_even));
            BOOST_HANA_CONSTEXPR_CHECK(all_of(list(2, 4), is_even));
            BOOST_HANA_CONSTEXPR_CHECK(not_(all_of(list(1, 2), is_even)));
            BOOST_HANA_CONSTEXPR_CHECK(not_(all_of(list(1, 3), is_even)));
            BOOST_HANA_CONSTEXPR_CHECK(not_(all_of(list(1, 3, 4), is_even)));


            //////////////////////////////////////////////////////////////////
            // all
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(all(list()));
            BOOST_HANA_CONSTEXPR_CHECK(all(list(logical(true))));
            BOOST_HANA_CONSTEXPR_CHECK(not_(all(list(logical(false)))));
            BOOST_HANA_CONSTEXPR_CHECK(all(list(logical(true), logical(true))));
            BOOST_HANA_CONSTEXPR_CHECK(not_(all(list(logical(true), logical(false)))));

            //////////////////////////////////////////////////////////////////
            // none_of
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(none_of(list(), invalid<>{}));
            BOOST_HANA_CONSTEXPR_CHECK(none_of(list(c(0)), equal.to(c(1))));
            BOOST_HANA_CONSTEXPR_CHECK(not_(none_of(list(c(0)), equal.to(c(0)))));

            BOOST_HANA_CONSTEXPR_CHECK(none_of(list(1), is_even));
            BOOST_HANA_CONSTEXPR_CHECK(not_(none_of(list(2), is_even)));
            BOOST_HANA_CONSTEXPR_CHECK(not_(none_of(list(1, 2), is_even)));
            BOOST_HANA_CONSTEXPR_CHECK(none_of(list(1, 3), is_even));
            BOOST_HANA_CONSTEXPR_CHECK(not_(none_of(list(1, 3, 4), is_even)));


            //////////////////////////////////////////////////////////////////
            // none
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(none(list()));
            BOOST_HANA_CONSTEXPR_CHECK(none(list(logical(false))));
            BOOST_HANA_CONSTEXPR_CHECK(not_(none(list(logical(true)))));

            //////////////////////////////////////////////////////////////////
            // find_if
            //////////////////////////////////////////////////////////////////
            {
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find_if(list(), equal.to(x<9>{})),
                    nothing
                ));

                BOOST_HANA_CONSTANT_CHECK(equal(
                    find_if(list(x<0>{}), equal.to(x<9>{})),
                    nothing
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find_if(list(x<0>{}), equal.to(x<0>{})),
                    just(x<0>{})
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find_if(list(x<0>{}, invalid<1>{}), equal.to(x<0>{})),
                    just(x<0>{})
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find_if(list(x<0>{}, invalid<1>{}, invalid<2>{}), equal.to(x<0>{})),
                    just(x<0>{})
                ));

                BOOST_HANA_CONSTANT_CHECK(equal(
                    find_if(list(x<0>{}, x<1>{}), equal.to(x<9>{})),
                    nothing
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find_if(list(x<0>{}, x<1>{}), equal.to(x<1>{})),
                    just(x<1>{})
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find_if(list(x<0>{}, x<1>{}, invalid<2>{}), equal.to(x<1>{})),
                    just(x<1>{})
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find_if(list(x<0>{}, x<1>{}, invalid<2>{}, invalid<3>{}), equal.to(x<1>{})),
                    just(x<1>{})
                ));

                BOOST_HANA_CONSTANT_CHECK(equal(
                    find_if(list(x<0>{}, x<1>{}, x<2>{}), equal.to(x<9>{})),
                    nothing
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find_if(list(x<0>{}, x<1>{}, x<2>{}), equal.to(x<2>{})),
                    just(x<2>{})
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find_if(list(x<0>{}, x<1>{}, x<2>{}, nothing), equal.to(x<2>{})),
                    just(x<2>{})
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find_if(list(x<0>{}, x<1>{}, x<2>{}, nothing, nothing), equal.to(x<2>{})),
                    just(x<2>{})
                ));

                // Make sure find_if works with an lvalue sequence. hana::tuple
                // used to have a bug that broke this.
                auto const const_lvalue = list(x<0>{}, x<1>{}, x<2>{});
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find_if(const_lvalue, equal.to(x<1>{})),
                    just(x<1>{})
                ));

                auto lvalue = list(x<0>{}, x<1>{}, x<2>{});
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find_if(lvalue, equal.to(x<1>{})),
                    just(x<1>{})
                ));
            }

            //////////////////////////////////////////////////////////////////
            // find
            //////////////////////////////////////////////////////////////////
            {
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find(list(), invalid<>{}),
                    nothing
                ));

                BOOST_HANA_CONSTANT_CHECK(equal(
                    find(list(x<0>{}), x<9>{}),
                    nothing
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find(list(x<0>{}), x<0>{}),
                    just(x<0>{})
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find(list(x<0>{}, invalid<1>{}), x<0>{}),
                    just(x<0>{})
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find(list(x<0>{}, invalid<1>{}, invalid<2>{}), x<0>{}),
                    just(x<0>{})
                ));

                BOOST_HANA_CONSTANT_CHECK(equal(
                    find(list(x<0>{}, x<1>{}), x<9>{}),
                    nothing
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find(list(x<0>{}, x<1>{}), x<1>{}),
                    just(x<1>{})
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find(list(x<0>{}, x<1>{}, invalid<2>{}), x<1>{}),
                    just(x<1>{})
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find(list(x<0>{}, x<1>{}, invalid<2>{}, invalid<3>{}), x<1>{}),
                    just(x<1>{})
                ));

                BOOST_HANA_CONSTANT_CHECK(equal(
                    find(list(x<0>{}, x<1>{}, x<2>{}), x<9>{}),
                    nothing
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find(list(x<0>{}, x<1>{}, x<2>{}), x<2>{}),
                    just(x<2>{})
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find(list(x<0>{}, x<1>{}, x<2>{}, nothing), x<2>{}),
                    just(x<2>{})
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    find(list(x<0>{}, x<1>{}, x<2>{}, nothing, nothing), x<2>{}),
                    just(x<2>{})
                ));
            }

            //////////////////////////////////////////////////////////////////
            // at_key
            //////////////////////////////////////////////////////////////////
            {
                BOOST_HANA_CONSTANT_CHECK(equal(
                    at_key(list(x<0>{}), x<0>{}),
                    x<0>{}
                ));

                BOOST_HANA_CONSTANT_CHECK(equal(
                    at_key(list(x<0>{}, x<1>{}), x<0>{}),
                    x<0>{}
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    at_key(list(x<0>{}, x<1>{}), x<1>{}),
                    x<1>{}
                ));

                BOOST_HANA_CONSTANT_CHECK(equal(
                    at_key(list(x<0>{}, x<1>{}, x<2>{}), x<0>{}),
                    x<0>{}
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    at_key(list(x<0>{}, x<1>{}, x<2>{}), x<1>{}),
                    x<1>{}
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    at_key(list(x<0>{}, x<1>{}, x<2>{}), x<2>{}),
                    x<2>{}
                ));
            }

            //////////////////////////////////////////////////////////////////
            // contains
            //////////////////////////////////////////////////////////////////
            {
                BOOST_HANA_CONSTANT_CHECK(not_(
                    contains(list(), undefined{})
                ));

                BOOST_HANA_CONSTANT_CHECK(not_(
                    contains(list(x<0>{}), x<999>{})
                ));

                BOOST_HANA_CONSTANT_CHECK(
                    contains(list(x<0>{}), x<0>{})
                );

                BOOST_HANA_CONSTANT_CHECK(
                    contains(list(x<0>{}, x<1>{}), x<1>{})
                );

                BOOST_HANA_CONSTANT_CHECK(
                    contains(list(x<0>{}, x<1>{}, x<3>{}), x<3>{})
                );

                BOOST_HANA_CONSTEXPR_CHECK(contains(list(c(0)), c(0)));
                BOOST_HANA_CONSTEXPR_CHECK(not_(contains(list(c(0)), c(1))));

                // infix application
                BOOST_HANA_CONSTANT_CHECK(
                    list(x<0>{}, x<1>{}) ^contains^ x<1>{}
                );
            }

            //////////////////////////////////////////////////////////////////
            // in
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTEXPR_CHECK(c(0) ^in^ list(c(0)));
            BOOST_HANA_CONSTEXPR_CHECK(not_(c(1) ^in^ list(c(0))));

            //////////////////////////////////////////////////////////////////
            // is_subset
            //////////////////////////////////////////////////////////////////
            {
                BOOST_HANA_CONSTANT_CHECK(is_subset(
                    list(), list(undefined{})
                ));

                BOOST_HANA_CONSTANT_CHECK(not_(is_subset(
                    list(undefined{}), list()
                )));

                BOOST_HANA_CONSTEXPR_CHECK(is_subset(
                    list(c(0)), list(c(0))
                ));

                BOOST_HANA_CONSTEXPR_CHECK(is_subset(
                    list(c(0)), list(c(0), c(1))
                ));

                BOOST_HANA_CONSTEXPR_CHECK(not_(is_subset(
                    list(c(0)), list(c(1))
                )));

                // infix application
                BOOST_HANA_CONSTANT_CHECK(
                    list(x<0>{}) ^is_subset^ list(x<1>{}, x<0>{})
                );

                BOOST_HANA_CONSTANT_CHECK(
                    list(x<0>{}, x<1>{}) ^is_subset^ list(x<1>{}, x<0>{})
                );
            }

            //////////////////////////////////////////////////////////////////
            // is_disjoint
            //////////////////////////////////////////////////////////////////
            {
                BOOST_HANA_CONSTANT_CHECK(is_disjoint(
                    list(),
                    list()
                ));

                BOOST_HANA_CONSTANT_CHECK(is_disjoint(
                    list(),
                    list(undefined{})
                ));
                BOOST_HANA_CONSTANT_CHECK(is_disjoint(
                    list(undefined{}),
                    list()
                ));

                BOOST_HANA_CONSTANT_CHECK(is_disjoint(
                    list(x<0>{}),
                    list(x<1>{})
                ));
                BOOST_HANA_CONSTANT_CHECK(is_disjoint(
                    list(x<1>{}),
                    list(x<0>{})
                ));
                BOOST_HANA_CONSTANT_CHECK(not_(is_disjoint(
                    list(x<0>{}),
                    list(x<0>{})
                )));


                BOOST_HANA_CONSTANT_CHECK(not_(is_disjoint(
                    list(x<0>{}, x<1>{}),
                    list(x<0>{})
                )));
                BOOST_HANA_CONSTANT_CHECK(not_(is_disjoint(
                    list(x<0>{}),
                    list(x<0>{}, x<1>{})
                )));
                BOOST_HANA_CONSTANT_CHECK(not_(is_disjoint(
                    list(x<1>{}, x<0>{}),
                    list(x<0>{}, x<1>{})
                )));
                BOOST_HANA_CONSTANT_CHECK(not_(is_disjoint(
                    list(x<0>{}, x<1>{}),
                    list(x<1>{}, x<2>{})
                )));
                BOOST_HANA_CONSTANT_CHECK(is_disjoint(
                    list(x<0>{}, x<1>{}),
                    list(x<2>{}, x<3>{})
                ));
            }
        }
    };
}}} // end namespace boost::hana::test

#endif // !BOOST_HANA_TEST_LAWS_SEARCHABLE_HPP
