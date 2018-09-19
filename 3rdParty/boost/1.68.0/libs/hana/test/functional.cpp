// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/functional.hpp>

#include <laws/base.hpp>
#include <support/tracked.hpp>

#include <utility>
namespace hana = boost::hana;


template <int i = 0>
struct nonpod : Tracked {
    nonpod() : Tracked{i} { }
};

template <int i = 0>
struct undefined { };

struct move_only {
    move_only(move_only&&) = default;
    move_only(move_only const&) = delete;
};


int main() {
    hana::test::_injection<0> f{};
    hana::test::_injection<1> g{};
    hana::test::_injection<2> h{};
    using hana::test::ct_eq;

    // always
    {
        auto z = ct_eq<0>{};
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::always(z)(), z
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::always(z)(undefined<1>{}), z
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::always(z)(undefined<1>{}, undefined<2>{}), z
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::always(z)(undefined<1>{}, undefined<2>{}, undefined<3>{}), z
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::always(z)(undefined<1>{}, undefined<2>{}, undefined<3>{}, undefined<4>{}), z
        ));

        hana::always(z)(nonpod<>{});
        auto m = hana::always(move_only{})(undefined<1>{}); (void)m;
    }

    // apply (tested separately)
    {

    }

    // arg
    {
        // moveonly friendliness:
        move_only z = hana::arg<1>(move_only{}); (void)z;

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::arg<1>(ct_eq<1>{}),
            ct_eq<1>{}
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::arg<1>(ct_eq<1>{}, undefined<2>{}),
            ct_eq<1>{}
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::arg<1>(ct_eq<1>{}, undefined<2>{}, undefined<3>{}),
            ct_eq<1>{}
        ));
        hana::arg<1>(nonpod<1>{});
        hana::arg<1>(nonpod<1>{}, nonpod<2>{});


        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::arg<2>(undefined<1>{}, ct_eq<2>{}),
            ct_eq<2>{}
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::arg<2>(undefined<1>{}, ct_eq<2>{}, undefined<3>{}),
            ct_eq<2>{}
        ));
        hana::arg<2>(nonpod<1>{}, nonpod<2>{});
        hana::arg<2>(nonpod<1>{}, nonpod<2>{}, nonpod<3>{});


        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::arg<3>(undefined<1>{}, undefined<2>{}, ct_eq<3>{}),
            ct_eq<3>{}
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::arg<3>(undefined<1>{}, undefined<2>{}, ct_eq<3>{}, undefined<4>{}),
            ct_eq<3>{}
        ));
        hana::arg<3>(nonpod<1>{}, nonpod<2>{}, nonpod<3>{});
        hana::arg<3>(nonpod<1>{}, nonpod<2>{}, nonpod<3>{}, nonpod<4>{});


        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::arg<4>(undefined<1>{}, undefined<2>{}, undefined<3>{}, ct_eq<4>{}),
            ct_eq<4>{}
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::arg<4>(undefined<1>{}, undefined<2>{}, undefined<3>{}, ct_eq<4>{}, undefined<5>{}),
            ct_eq<4>{}
        ));
        hana::arg<4>(nonpod<1>{}, nonpod<2>{}, nonpod<3>{}, nonpod<4>{});
        hana::arg<4>(nonpod<1>{}, nonpod<2>{}, nonpod<3>{}, nonpod<4>{}, nonpod<5>{});


        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::arg<5>(undefined<1>{}, undefined<2>{}, undefined<3>{}, undefined<4>{}, ct_eq<5>{}),
            ct_eq<5>{}
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::arg<5>(undefined<1>{}, undefined<2>{}, undefined<3>{}, undefined<4>{}, ct_eq<5>{}, undefined<6>{}),
            ct_eq<5>{}
        ));
        hana::arg<5>(nonpod<1>{}, nonpod<2>{}, nonpod<3>{}, nonpod<4>{}, nonpod<5>{});
        hana::arg<5>(nonpod<1>{}, nonpod<2>{}, nonpod<3>{}, nonpod<4>{}, nonpod<5>{}, nonpod<6>{});


        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::arg<6>(undefined<1>{}, undefined<2>{}, undefined<3>{}, undefined<4>{}, undefined<5>{}, ct_eq<6>{}),
            ct_eq<6>{}
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::arg<6>(undefined<1>{}, undefined<2>{}, undefined<3>{}, undefined<4>{}, undefined<5>{}, ct_eq<6>{}, undefined<7>{}),
            ct_eq<6>{}
        ));
        hana::arg<6>(nonpod<1>{}, nonpod<2>{}, nonpod<3>{}, nonpod<4>{}, nonpod<5>{}, nonpod<6>{});
        hana::arg<6>(nonpod<1>{}, nonpod<2>{}, nonpod<3>{}, nonpod<4>{}, nonpod<5>{}, nonpod<6>{}, nonpod<7>{});
    }

    // compose
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::compose(f, g)(ct_eq<0>{}),
            f(g(ct_eq<0>{}))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::compose(f, g)(ct_eq<0>{}, ct_eq<1>{}),
            f(g(ct_eq<0>{}), ct_eq<1>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::compose(f, g)(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
            f(g(ct_eq<0>{}), ct_eq<1>{}, ct_eq<2>{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::compose(f, g, h)(ct_eq<0>{}),
            f(g(h(ct_eq<0>{})))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::compose(f, g, h)(ct_eq<0>{}, ct_eq<1>{}),
            f(g(h(ct_eq<0>{})), ct_eq<1>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::compose(f, g, h)(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
            f(g(h(ct_eq<0>{})), ct_eq<1>{}, ct_eq<2>{})
        ));

        auto h = [capture = move_only{}](int) { (void)capture; return 1; };
        auto i = [](int) { return 1; };
        hana::compose(std::move(h), i)(1);

        {
            // Compose move-only functions.
            auto f = [capture = move_only{}](int) { (void)capture; return 1; };
            auto g = [capture = move_only{}](int) { (void)capture; return 1; };
            auto c = hana::compose(std::move(f), std::move(g)); (void)c;
        }
    }

    // curry
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::curry<0>(f)(),
            f()
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::curry<1>(f)(ct_eq<1>{}),
            f(ct_eq<1>{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::curry<2>(f)(ct_eq<1>{})(ct_eq<2>{}),
            f(ct_eq<1>{}, ct_eq<2>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::curry<2>(f)(ct_eq<1>{}, ct_eq<2>{}),
            f(ct_eq<1>{}, ct_eq<2>{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::curry<3>(f)(ct_eq<1>{})(ct_eq<2>{})(ct_eq<3>{}),
            f(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::curry<3>(f)(ct_eq<1>{})(ct_eq<2>{}, ct_eq<3>{}),
            f(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::curry<3>(f)(ct_eq<1>{}, ct_eq<2>{})(ct_eq<3>{}),
            f(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::curry<3>(f)(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
            f(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        ));


        // Make sure curry is idempotent; this is important because it allows
        // currying a function in generic contexts where it is unknown whether
        // the function is already curried.
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::curry<0>(hana::curry<0>(f))(),
            f()
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::curry<1>(hana::curry<1>(f))(ct_eq<1>{}),
            f(ct_eq<1>{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::curry<2>(hana::curry<2>(f))(ct_eq<1>{})(ct_eq<2>{}),
            f(ct_eq<1>{}, ct_eq<2>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::curry<2>(hana::curry<2>(f))(ct_eq<1>{}, ct_eq<2>{}),
            f(ct_eq<1>{}, ct_eq<2>{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::curry<3>(hana::curry<3>(f))(ct_eq<1>{})(ct_eq<2>{})(ct_eq<3>{}),
            f(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::curry<3>(hana::curry<3>(f))(ct_eq<1>{})(ct_eq<2>{}, ct_eq<3>{}),
            f(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::curry<3>(hana::curry<3>(f))(ct_eq<1>{}, ct_eq<2>{})(ct_eq<3>{}),
            f(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::curry<3>(hana::curry<3>(f))(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
            f(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        ));
    }

    // demux (tested separately)
    {

    }

    // fix (tested separately)
    {

    }

    // flip
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::flip(f)(ct_eq<1>{}, ct_eq<2>{}),
            f(ct_eq<2>{}, ct_eq<1>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::flip(f)(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
            f(ct_eq<2>{}, ct_eq<1>{}, ct_eq<3>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::flip(f)(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}),
            f(ct_eq<2>{}, ct_eq<1>{}, ct_eq<3>{}, ct_eq<4>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::flip(f)(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}),
            f(ct_eq<2>{}, ct_eq<1>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{})
        ));
    }

    // id
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::id(ct_eq<0>{}),
            ct_eq<0>{}
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::id(ct_eq<1>{}),
            ct_eq<1>{}
        ));

        (void)hana::id(move_only{});

        // make sure we don't return a dangling reference
        auto f = []() -> decltype(auto) { return hana::id(Tracked{1}); };
        auto z = f(); (void)z;
    }

    // lockstep (tested separately)
    {

    }

    // infix
    {
        auto g = hana::infix(f);

        // disregard associativity
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            ct_eq<0>{} ^g^ ct_eq<1>{},
            f(ct_eq<0>{}, ct_eq<1>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            (ct_eq<0>{} ^g)^ ct_eq<1>{},
            f(ct_eq<0>{}, ct_eq<1>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            ct_eq<0>{} ^(g^ ct_eq<1>{}),
            f(ct_eq<0>{}, ct_eq<1>{})
        ));

        // left partial application
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            (ct_eq<0>{}^g)(ct_eq<1>{}),
            f(ct_eq<0>{}, ct_eq<1>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            (ct_eq<0>{}^g)(ct_eq<1>{}, ct_eq<2>{}),
            f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            (ct_eq<0>{}^g)(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
            f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        ));

        // right partial application
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            (g^ct_eq<1>{})(ct_eq<0>{}),
            f(ct_eq<0>{}, ct_eq<1>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            (g^ct_eq<2>{})(ct_eq<0>{}, ct_eq<1>{}),
            f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            (g^ct_eq<3>{})(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
            f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        ));

        // equivalence with the base function
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            g(ct_eq<0>{}, ct_eq<1>{}),
            f(ct_eq<0>{}, ct_eq<1>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            g(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
            f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            g(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
            f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        ));
    }

    // on
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::on(f, g)(),
            f()
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::on(f, g)(ct_eq<0>{}),
            f(g(ct_eq<0>{}))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::on(f, g)(ct_eq<0>{}, ct_eq<1>{}),
            f(g(ct_eq<0>{}), g(ct_eq<1>{}))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::on(f, g)(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
            f(g(ct_eq<0>{}), g(ct_eq<1>{}), g(ct_eq<2>{}))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::on(f, g)(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
            f(g(ct_eq<0>{}), g(ct_eq<1>{}), g(ct_eq<2>{}), g(ct_eq<3>{}))
        ));

        // check the infix version
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            (f ^hana::on^ g)(),
            f()
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            (f ^hana::on^ g)(ct_eq<0>{}),
            f(g(ct_eq<0>{}))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            (f ^hana::on^ g)(ct_eq<0>{}, ct_eq<1>{}),
            f(g(ct_eq<0>{}), g(ct_eq<1>{}))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            (f ^hana::on^ g)(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
            f(g(ct_eq<0>{}), g(ct_eq<1>{}), g(ct_eq<2>{}))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            (f ^hana::on^ g)(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
            f(g(ct_eq<0>{}), g(ct_eq<1>{}), g(ct_eq<2>{}), g(ct_eq<3>{}))
        ));
    }

    // overload
    {
        // 1 function
        {
            auto f = hana::overload([](int) { return ct_eq<1>{}; });
            BOOST_HANA_CONSTANT_CHECK(hana::equal(f(int{}), ct_eq<1>{}));
        }

        // 2 functions
        {
            auto f = hana::overload(
                [](int) { return ct_eq<1>{}; },
                [](float) { return ct_eq<2>{}; }
            );
            BOOST_HANA_CONSTANT_CHECK(hana::equal(f(int{}), ct_eq<1>{}));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(f(float{}), ct_eq<2>{}));
        }

        // 3 functions
        {
            auto f = hana::overload(
                [](int) { return ct_eq<1>{}; },
                [](float) { return ct_eq<2>{}; },
                static_cast<ct_eq<3>(*)(char)>([](char) { return ct_eq<3>{}; })
            );
            BOOST_HANA_CONSTANT_CHECK(hana::equal(f(int{}), ct_eq<1>{}));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(f(float{}), ct_eq<2>{}));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(f(char{}), ct_eq<3>{}));
        }

        // 4 functions
        {
            auto f = hana::overload(
                [](int) { return ct_eq<1>{}; },
                [](float) { return ct_eq<2>{}; },
                static_cast<ct_eq<3>(*)(char)>([](char) { return ct_eq<3>{}; }),
                [](auto) { return ct_eq<4>{}; }
            );

            struct otherwise { };
            BOOST_HANA_CONSTANT_CHECK(hana::equal(f(int{}), ct_eq<1>{}));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(f(float{}), ct_eq<2>{}));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(f(char{}), ct_eq<3>{}));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(f(otherwise{}), ct_eq<4>{}));
        }

        // check move-only friendliness for bare functions
        {
            void (*g)(move_only) = [](move_only) { };
            hana::overload(g)(move_only{});
        }

        // check non-strict matches which require a conversion
        {
            struct convertible_to_int { operator int() const { return 1; } };
            auto f = [](int) { return ct_eq<0>{}; };

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::overload(f)(convertible_to_int{}),
                ct_eq<0>{}
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::overload(static_cast<ct_eq<0>(*)(int)>(f))(convertible_to_int{}),
                ct_eq<0>{}
            ));
        }
    }

    // partial (tested separately)
    {

    }

    // placeholder (tested separately)
    {

    }
}
