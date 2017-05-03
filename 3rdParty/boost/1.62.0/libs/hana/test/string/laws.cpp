// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/string.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/comparable.hpp>
#include <laws/foldable.hpp>
#include <laws/hashable.hpp>
#include <laws/iterable.hpp>
#include <laws/orderable.hpp>
#include <laws/searchable.hpp>
namespace hana = boost::hana;


int main() {
    // Comparable and Hashable
    {
        auto strings = hana::make_tuple(
            BOOST_HANA_STRING(""),
            BOOST_HANA_STRING("a"),
            BOOST_HANA_STRING("ab"),
            BOOST_HANA_STRING("abc"),
            BOOST_HANA_STRING("abcd"),
            BOOST_HANA_STRING("abcde"),
            BOOST_HANA_STRING("ba")
        );

        hana::test::TestComparable<hana::string_tag>{strings};
        hana::test::TestHashable<hana::string_tag>{strings};
    }

    // Foldable and Iterable
    {
        auto strings = hana::make_tuple(
            BOOST_HANA_STRING(""),
            BOOST_HANA_STRING("a"),
            BOOST_HANA_STRING("ab"),
            BOOST_HANA_STRING("abc"),
            BOOST_HANA_STRING("abcd"),
            BOOST_HANA_STRING("abcde"),
            BOOST_HANA_STRING("ba"),
            BOOST_HANA_STRING("afcd")
        );

        hana::test::TestFoldable<hana::string_tag>{strings};
        hana::test::TestIterable<hana::string_tag>{strings};
    }

    // Orderable
    {
        auto strings = hana::make_tuple(
            BOOST_HANA_STRING(""),
            BOOST_HANA_STRING("a"),
            BOOST_HANA_STRING("ab"),
            BOOST_HANA_STRING("abc"),
            BOOST_HANA_STRING("ba"),
            BOOST_HANA_STRING("abd")
        );

        hana::test::TestOrderable<hana::string_tag>{strings};
    }

    // Searchable
    {
        auto keys = hana::tuple_c<char, 'a', 'f'>;
        auto strings = hana::make_tuple(
            BOOST_HANA_STRING(""),
            BOOST_HANA_STRING("a"),
            BOOST_HANA_STRING("ab"),
            BOOST_HANA_STRING("abcd"),
            BOOST_HANA_STRING("ba"),
            BOOST_HANA_STRING("afcd")
        );

        hana::test::TestSearchable<hana::string_tag>{strings, keys};
    }
}
