// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_LAWS_SEQUENCE_HPP
#define BOOST_HANA_TEST_LAWS_SEQUENCE_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/concept/sequence.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/core/tag_of.hpp>
#include <boost/hana/functional/capture.hpp>
#include <boost/hana/functional/compose.hpp>
#include <boost/hana/functional/id.hpp>
#include <boost/hana/functional/partial.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/plus.hpp>
#include <boost/hana/range.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
#include <support/minimal_product.hpp>
#include <support/seq.hpp>

#include <type_traits>
#include <vector>


#if defined(BOOST_HANA_TEST_SEQUENCE_KNOWN_PARTS) && \
    BOOST_HANA_TEST_SEQUENCE_KNOWN_PARTS != 3
#   error The Sequence laws are currently split into 3 different parts. \
          Your unit test must have gone out of sync.
#endif

#if defined(BOOST_HANA_TEST_SEQUENCE_PART) && \
    !defined(BOOST_HANA_TEST_SEQUENCE_KNOWN_PARTS)
#   error When splitting the Sequence test into subparts, you must define \
          the BOOST_HANA_TEST_SEQUENCE_KNOWN_PARTS macro to the number of \
          parts in the Sequence test to make sure your tests stay in sync.
#endif

#if defined(BOOST_HANA_TEST_SEQUENCE_PART) && BOOST_HANA_TEST_SEQUENCE_PART > 3
#   error BOOST_HANA_TEST_SEQUENCE_PART must not be defined or              \
          it must be a number in {1, 2, 3}
#endif

namespace boost { namespace hana { namespace test {
    template <typename S, typename = when<true>>
    struct TestSequence : TestSequence<S, laws> {
        using TestSequence<S, laws>::TestSequence;
    };

    template <typename S>
    struct TestSequence<S, laws> {
        template <int i>
        using eq = integer<i,
              Policy::Comparable
            | Policy::Constant
        >;

        template <int i>
        using cx_eq = integer<i,
              Policy::Comparable
            | Policy::Constexpr
        >;

        template <int i>
        using ord = integer<i,
              Policy::Orderable
            | Policy::Constant
        >;

        struct undefined { };

        TestSequence() {
            constexpr auto list = make<S>; (void)list;
            constexpr auto foldable = ::seq; (void)foldable;

#if !defined(BOOST_HANA_TEST_SEQUENCE_PART) || BOOST_HANA_TEST_SEQUENCE_PART == 1
            //////////////////////////////////////////////////////////////////
            // Check for Sequence<...>
            //////////////////////////////////////////////////////////////////
            static_assert(Sequence<decltype(list())>::value, "");
            static_assert(Sequence<decltype(list(1))>::value, "");
            static_assert(Sequence<decltype(list(1, '2'))>::value, "");
            static_assert(Sequence<decltype(list(1, '2', 3.4))>::value, "");

            //////////////////////////////////////////////////////////////////
            // Check for basic tag consistency
            //////////////////////////////////////////////////////////////////
            struct Random;
            static_assert(std::is_same<tag_of_t<decltype(list())>, S>{}, "");
            static_assert(std::is_same<tag_of_t<decltype(list(1))>, S>{}, "");
            static_assert(std::is_same<tag_of_t<decltype(list(1, '2'))>, S>{}, "");
            static_assert(std::is_same<tag_of_t<decltype(list(1, '2', 3.3))>, S>{}, "");
            static_assert(!std::is_same<tag_of_t<Random>, S>{}, "");

            //////////////////////////////////////////////////////////////////
            // Foldable -> Sequence conversion
            //////////////////////////////////////////////////////////////////
            {
                BOOST_HANA_CONSTANT_CHECK(equal(
                    to<S>(foldable()),
                    list()
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    to<S>(foldable(eq<0>{})),
                    list(eq<0>{})
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    to<S>(foldable(eq<0>{}, eq<1>{})),
                    list(eq<0>{}, eq<1>{})
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    to<S>(foldable(eq<0>{}, eq<1>{}, eq<2>{})),
                    list(eq<0>{}, eq<1>{}, eq<2>{})
                ));
                BOOST_HANA_CONSTANT_CHECK(equal(
                    to<S>(foldable(eq<0>{}, eq<1>{}, eq<2>{}, eq<3>{})),
                    list(eq<0>{}, eq<1>{}, eq<2>{}, eq<3>{})
                ));
            }

            //////////////////////////////////////////////////////////////////
            // make (tautological given our definition of `list`)
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(equal(
                make<S>(),
                list()
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                make<S>(eq<0>{}),
                list(eq<0>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                make<S>(eq<0>{}, eq<1>{}),
                list(eq<0>{}, eq<1>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                make<S>(eq<0>{}, eq<1>{}, eq<2>{}),
                list(eq<0>{}, eq<1>{}, eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                make<S>(eq<0>{}, eq<1>{}, eq<2>{}, eq<3>{}),
                list(eq<0>{}, eq<1>{}, eq<2>{}, eq<3>{})
            ));
#endif
        }
    };
}}} // end namespace boost::hana::test

#endif // !BOOST_HANA_TEST_LAWS_SEQUENCE_HPP
