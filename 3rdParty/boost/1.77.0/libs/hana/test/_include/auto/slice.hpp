// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_SLICE_HPP
#define BOOST_HANA_TEST_AUTO_SLICE_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/range.hpp>
#include <boost/hana/slice.hpp>
#include <boost/hana/tuple.hpp>

#include "test_case.hpp"
#include <laws/base.hpp>
#include <support/seq.hpp>

#include <cstddef>


TestCase test_slice{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;
    constexpr auto foldable = ::seq;

    struct undefined { };

    //////////////////////////////////////////////////////////////////////////
    // Test with arbitrary indices
    //////////////////////////////////////////////////////////////////////////
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(),
                    foldable()),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(undefined{}),
                    foldable()),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(undefined{}, undefined{}),
                    foldable()),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(ct_eq<0>{}),
                    foldable(hana::size_c<0>)),
        MAKE_TUPLE(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(ct_eq<0>{}, undefined{}),
                    foldable(hana::size_c<0>)),
        MAKE_TUPLE(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(undefined{}, ct_eq<1>{}),
                    foldable(hana::size_c<1>)),
        MAKE_TUPLE(ct_eq<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}),
                    foldable(hana::size_c<0>, hana::size_c<1>)),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}),
                    foldable(hana::size_c<1>, hana::size_c<0>)),
        MAKE_TUPLE(ct_eq<1>{}, ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}),
                    foldable(hana::size_c<0>, hana::size_c<0>)),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}),
                    foldable(hana::size_c<1>, hana::size_c<1>)),
        MAKE_TUPLE(ct_eq<1>{}, ct_eq<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
              foldable(hana::size_c<0>, hana::size_c<1>, hana::size_c<2>)),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
                    foldable(hana::size_c<0>, hana::size_c<2>, hana::size_c<1>)),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<2>{}, ct_eq<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
                    foldable(hana::size_c<0>, hana::size_c<2>, hana::size_c<1>, hana::size_c<0>)),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<2>{}, ct_eq<1>{}, ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
                    foldable(hana::size_c<0>, hana::size_c<2>, hana::size_c<1>, hana::size_c<0>, hana::size_c<1>)),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<2>{}, ct_eq<1>{}, ct_eq<0>{}, ct_eq<1>{})
    ));

    // Try with a tuple_c
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
                    hana::tuple_c<unsigned, 1, 3, 2>),
        MAKE_TUPLE(ct_eq<1>{}, ct_eq<3>{}, ct_eq<2>{})
    ));



    //////////////////////////////////////////////////////////////////////////
    // Test with a `range` (check the optimization for contiguous indices)
    //////////////////////////////////////////////////////////////////////////
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(),
                    hana::range_c<std::size_t, 0, 0>),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(undefined{}),
                    hana::range_c<std::size_t, 0, 0>),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(undefined{}, undefined{}),
                    hana::range_c<std::size_t, 0, 0>),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(ct_eq<0>{}),
                    hana::range_c<std::size_t, 0, 1>),
        MAKE_TUPLE(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(ct_eq<0>{}, undefined{}),
                    hana::range_c<std::size_t, 0, 1>),
        MAKE_TUPLE(ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(undefined{}, ct_eq<1>{}),
                    hana::range_c<std::size_t, 1, 2>),
        MAKE_TUPLE(ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(undefined{}, ct_eq<1>{}, undefined{}),
                    hana::range_c<std::size_t, 1, 2>),
        MAKE_TUPLE(ct_eq<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}),
                    hana::range_c<std::size_t, 0, 2>),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, undefined{}),
                    hana::range_c<std::size_t, 0, 2>),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(undefined{}, ct_eq<1>{}, ct_eq<2>{}),
                    hana::range_c<std::size_t, 1, 3>),
        MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::slice(MAKE_TUPLE(undefined{}, ct_eq<1>{}, ct_eq<2>{}, undefined{}),
                    hana::range_c<std::size_t, 1, 3>),
        MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{})
    ));
}};

#endif // !BOOST_HANA_TEST_AUTO_SLICE_HPP
