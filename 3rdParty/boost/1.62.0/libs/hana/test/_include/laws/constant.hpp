// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_LAWS_CONSTANT_HPP
#define BOOST_HANA_TEST_LAWS_CONSTANT_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/concept/comparable.hpp>
#include <boost/hana/core/when.hpp>
#include <boost/hana/functional/capture.hpp>
#include <boost/hana/concept/logical.hpp>

#include <laws/base.hpp>
#include <laws/comparable.hpp>
#include <laws/euclidean_ring.hpp>
#include <laws/group.hpp>
#include <laws/logical.hpp>
#include <laws/monoid.hpp>
#include <laws/orderable.hpp>
#include <laws/ring.hpp>

#include <type_traits>


namespace boost { namespace hana { namespace test {
    template <typename C, typename = when<true>>
    struct TestConstant {
        using T = typename C::value_type;

        template <typename X>
        struct wrap_arbitrary_constant {
            static constexpr bool value = boost::hana::value<X>();
            using hana_tag = detail::CanonicalConstant<T>;
        };

        template <typename Xs, typename Convertibles>
        TestConstant(Xs xs, Convertibles types) {
            hana::for_each(xs, [](auto x) {
                static_assert(Constant<decltype(x)>::value, "");
            });

            hana::for_each(xs, hana::capture(types)([](auto types, auto c) {

                // constexpr-ness of hana::value(c)
                constexpr auto must_be_constexpr1 = hana::value(c);
                constexpr auto must_be_constexpr2 = hana::value<decltype(c)>();
                (void)must_be_constexpr1;
                (void)must_be_constexpr2;

                // consistency of C::value_type
                static_assert(std::is_same<
                    T,
                    tag_of_t<decltype(hana::value(c))>
                >{}, "");

                // equivalence of value_of(c) and value<decltype(c)>
                BOOST_HANA_CHECK(hana::equal(
                    hana::value_of(c),
                    hana::value<decltype(c)>()
                ));

                // equivalence of value<decltype(c)>() and value(c)
                BOOST_HANA_CHECK(hana::equal(
                    hana::value<decltype(c)>(),
                    hana::value(c)
                ));

                // conversion from an arbitrary Constant
                (void)to<C>(wrap_arbitrary_constant<decltype(c)>{});
                static_assert(is_embedded<detail::CanonicalConstant<T>, C>{}, "");

                hana::for_each(types, hana::capture(c)([](auto c, auto u) {
                    using U = typename decltype(u)::type;

                    // conversion to something to which the underlying data
                    // type can be converted.
                    BOOST_HANA_CHECK(equal(
                        to<U>(c),
                        make<U>(hana::value(c))
                    ));
                    static_assert(is_embedded<C, U>::value ^iff^ is_embedded<T, U>::value, "");

                    // common data type
                    static_assert(std::is_same<
                        common_t<C, detail::CanonicalConstant<U>>,
                        detail::CanonicalConstant<common_t<T, U>>
                    >{}, "");

                    static_assert(std::is_same<
                        common_t<detail::CanonicalConstant<U>, C>,
                        detail::CanonicalConstant<common_t<T, U>>
                    >{}, "");

                    static_assert(std::is_same<
                        common_t<C, U>,
                        common_t<typename C::value_type, U>
                    >{}, "");
                }));
            }));
        }
    };
}}} // end namespace boost::hana::test

#endif // !BOOST_HANA_TEST_LAWS_CONSTANT_HPP
