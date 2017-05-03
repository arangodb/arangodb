// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_EXT_BOOST_FUSION_TESTS_HPP
#define BOOST_HANA_TEST_EXT_BOOST_FUSION_TESTS_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/at.hpp>
#include <boost/hana/concept/sequence.hpp>
#include <boost/hana/core/make.hpp>
#include <boost/hana/drop_front.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/is_empty.hpp>
#include <boost/hana/length.hpp>
#include <boost/hana/not.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


//
// Before including this header, define the following macros:
//
//  MAKE_TUPLE(...)
//      Must expand to a sequence holding __VA_ARGS__. A valid definition
//      would be hana::make_tuple(__VA_ARGS__).
//
//  TUPLE_TYPE(...)
//      Must expand to the type of a sequence holding objects of type __VA_ARGS__.
//      A valid definition would be hana::tuple<__VA_ARGS__>.
//
//  TUPLE_TAG
//      Must expand to the tag of the sequence. A valid definition would
//      be hana::tuple_tag.
//


struct undefined { };

int main() {
    //////////////////////////////////////////////////////////////////////////
    // make<...>
    //////////////////////////////////////////////////////////////////////////
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            MAKE_TUPLE(),
            hana::make<TUPLE_TAG>()
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            MAKE_TUPLE(ct_eq<0>{}),
            hana::make<TUPLE_TAG>(ct_eq<0>{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}),
            hana::make<TUPLE_TAG>(ct_eq<0>{}, ct_eq<1>{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
            hana::make<TUPLE_TAG>(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
        ));
    }

    //////////////////////////////////////////////////////////////////////////
    // drop_front
    //////////////////////////////////////////////////////////////////////////
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front(MAKE_TUPLE(ct_eq<0>{})),
            MAKE_TUPLE()
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})),
            MAKE_TUPLE(ct_eq<1>{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
            MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})),
            MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        ));


        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), hana::size_c<2>),
            MAKE_TUPLE(ct_eq<2>{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}), hana::size_c<2>),
            MAKE_TUPLE(ct_eq<2>{}, ct_eq<3>{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}), hana::size_c<3>),
            MAKE_TUPLE(ct_eq<3>{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}), hana::size_c<5>),
            MAKE_TUPLE()
        ));
    }

    //////////////////////////////////////////////////////////////////////////
    // at
    //////////////////////////////////////////////////////////////////////////
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::at_c<0>(MAKE_TUPLE(ct_eq<0>{})),
            ct_eq<0>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::at_c<0>(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})),
            ct_eq<0>{}
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::at_c<1>(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})),
            ct_eq<1>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::at_c<0>(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
            ct_eq<0>{}
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::at_c<1>(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
            ct_eq<1>{}
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::at_c<2>(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
            ct_eq<2>{}
        ));
    }

    //////////////////////////////////////////////////////////////////////////
    // is_empty
    //////////////////////////////////////////////////////////////////////////
    {
        BOOST_HANA_CONSTANT_CHECK(hana::is_empty(
            MAKE_TUPLE()
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(
            MAKE_TUPLE(undefined{})
        )));

        BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(
            MAKE_TUPLE(undefined{}, undefined{})
        )));
    }

    //////////////////////////////////////////////////////////////////////////
    // length
    //////////////////////////////////////////////////////////////////////////
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::length(MAKE_TUPLE()),
            hana::size_c<0>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::length(MAKE_TUPLE(undefined{})),
            hana::size_c<1>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::length(MAKE_TUPLE(undefined{}, undefined{})),
            hana::size_c<2>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::length(MAKE_TUPLE(undefined{}, undefined{}, undefined{})),
            hana::size_c<3>
        ));
    }

    //////////////////////////////////////////////////////////////////////////
    // Sequence
    //////////////////////////////////////////////////////////////////////////
    {
        static_assert(hana::Sequence<TUPLE_TAG>::value, "");
    }
}

#endif // !BOOST_HANA_TEST_EXT_BOOST_FUSION_TESTS_HPP
