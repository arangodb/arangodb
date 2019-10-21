// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/any_of.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/concept/foldable.hpp>
#include <boost/hana/concept/searchable.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/find_if.hpp>
#include <boost/hana/functional/always.hpp>
#include <boost/hana/functional/placeholder.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/unpack.hpp>

#include <laws/base.hpp>
#include <laws/foldable.hpp>
#include <laws/searchable.hpp>

#include <cstddef>
namespace hana = boost::hana;


template <typename T, std::size_t n>
using array = T[n];

int main() {
    // We can't check the laws because builtin arrays can't be passed
    // to functions.

    //////////////////////////////////////////////////////////////////////////
    // Foldable
    //////////////////////////////////////////////////////////////////////////
    {
        int a[] = {1};
        int b[] = {1, 2};
        int c[] = {1, 2, 3};
        int d[] = {1, 2, 3, 4};

        // unpack
        {
            hana::test::_injection<0> f{};

            BOOST_HANA_RUNTIME_CHECK(hana::equal(
                hana::unpack(a, f),
                f(1)
            ));

            BOOST_HANA_RUNTIME_CHECK(hana::equal(
                hana::unpack(b, f),
                f(1, 2)
            ));

            BOOST_HANA_RUNTIME_CHECK(hana::equal(
                hana::unpack(c, f),
                f(1, 2, 3)
            ));

            BOOST_HANA_RUNTIME_CHECK(hana::equal(
                hana::unpack(d, f),
                f(1, 2, 3, 4)
            ));
        }

        static_assert(hana::Foldable<int[3]>::value, "");
    }

    //////////////////////////////////////////////////////////////////////////
    // Searchable
    //////////////////////////////////////////////////////////////////////////
    {
        // any_of
        {
            static_assert(
                hana::not_(hana::any_of(array<int, 1>{0}, hana::equal.to(1)))
            , "");

            static_assert(
                hana::any_of(array<int, 2>{0, 1}, hana::equal.to(0))
            , "");
            static_assert(
                hana::any_of(array<int, 2>{0, 1}, hana::equal.to(1))
            , "");
            static_assert(
                hana::not_(hana::any_of(array<int, 2>{0, 1}, hana::equal.to(2)))
            , "");

            static_assert(
                hana::any_of(array<int, 3>{0, 1, 2}, hana::equal.to(0))
            , "");
            static_assert(
                hana::any_of(array<int, 3>{0, 1, 2}, hana::equal.to(1))
            , "");
            static_assert(
                hana::any_of(array<int, 3>{0, 1, 2}, hana::equal.to(2))
            , "");
            static_assert(
                hana::not_(hana::any_of(array<int, 3>{0, 1, 2}, hana::equal.to(3)))
            , "");
        }

        // find_if
        // Note: Because we need the predicate to return a Constant, this
        // is incredibly not powerful.
        {
            static_assert(hana::equal(
                hana::find_if(array<int, 1>{0}, hana::always(hana::true_c)),
                hana::just(0)
            ), "");

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::find_if(array<int, 1>{0}, hana::always(hana::false_c)),
                hana::nothing
            ));
        }

        static_assert(hana::Searchable<int[3]>::value, "");
    }
}
