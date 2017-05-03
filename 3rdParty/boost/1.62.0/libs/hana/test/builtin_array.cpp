// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/concept/comparable.hpp>
#include <boost/hana/concept/foldable.hpp>
#include <boost/hana/functional/always.hpp>
#include <boost/hana/functional/placeholder.hpp>
#include <boost/hana/concept/logical.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/concept/searchable.hpp>

#include <laws/base.hpp>
#include <laws/foldable.hpp>
#include <laws/searchable.hpp>

#include <cstddef>
using namespace boost::hana;


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
            test::_injection<0> f{};

            BOOST_HANA_RUNTIME_CHECK(equal(
                unpack(a, f),
                f(1)
            ));

            BOOST_HANA_RUNTIME_CHECK(equal(
                unpack(b, f),
                f(1, 2)
            ));

            BOOST_HANA_RUNTIME_CHECK(equal(
                unpack(c, f),
                f(1, 2, 3)
            ));

            BOOST_HANA_RUNTIME_CHECK(equal(
                unpack(d, f),
                f(1, 2, 3, 4)
            ));
        }

        static_assert(Foldable<int[3]>::value, "");
    }

    //////////////////////////////////////////////////////////////////////////
    // Searchable
    //////////////////////////////////////////////////////////////////////////
    {
        // any_of
        {
            static_assert(
                not_(any_of(array<int, 1>{0}, _ == 1))
            , "");

            static_assert(
                any_of(array<int, 2>{0, 1}, _ == 0)
            , "");
            static_assert(
                any_of(array<int, 2>{0, 1}, _ == 1)
            , "");
            static_assert(
                not_(any_of(array<int, 2>{0, 1}, _ == 2))
            , "");

            static_assert(
                any_of(array<int, 3>{0, 1, 2}, _ == 0)
            , "");
            static_assert(
                any_of(array<int, 3>{0, 1, 2}, _ == 1)
            , "");
            static_assert(
                any_of(array<int, 3>{0, 1, 2}, _ == 2)
            , "");
            static_assert(
                not_(any_of(array<int, 3>{0, 1, 2}, _ == 3))
            , "");
        }

        // find_if
        // Note: Because we need the predicate to return a Constant, this
        // is incredibly not powerful.
        {
            static_assert(equal(
                find_if(array<int, 1>{0}, always(true_c)),
                just(0)
            ), "");

            BOOST_HANA_CONSTANT_CHECK(equal(
                find_if(array<int, 1>{0}, always(false_c)),
                nothing
            ));
        }

        static_assert(Searchable<int[3]>::value, "");
    }
}
