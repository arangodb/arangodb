// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/ext/std/integer_sequence.hpp>
#include <boost/hana/ext/std/integral_constant.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/comparable.hpp>
#include <laws/foldable.hpp>
#include <laws/iterable.hpp>
#include <laws/searchable.hpp>

#include <type_traits>
#include <utility>
namespace hana = boost::hana;


auto sequences = hana::make_tuple(
    std::integer_sequence<int>{},
    std::integer_sequence<int, 2>{},
    std::integer_sequence<int, 3>{},
    std::integer_sequence<int, 3, 4>{},
    std::integer_sequence<int, 3, 4, 5>{}
);

auto keys = hana::make_tuple(
    std::integral_constant<int, 3>{}, std::integral_constant<long long, 4>{}
);

int main() {
    hana::test::TestComparable<hana::ext::std::integer_sequence_tag>{sequences};
    hana::test::TestFoldable<hana::ext::std::integer_sequence_tag>{sequences};
    hana::test::TestIterable<hana::ext::std::integer_sequence_tag>{sequences};
    hana::test::TestSearchable<hana::ext::std::integer_sequence_tag>{sequences, keys};
}
