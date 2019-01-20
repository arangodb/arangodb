// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/integral_constant.hpp>
#include <boost/hana/range.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/comparable.hpp>
#include <laws/foldable.hpp>
#include <laws/iterable.hpp>
#include <laws/searchable.hpp>

#include <type_traits>
namespace hana = boost::hana;


int main() {
    auto ranges = hana::make_tuple(
          hana::make_range(hana::int_c<0>, hana::int_c<0>)
        , hana::make_range(hana::int_c<0>, hana::int_c<1>)
        , hana::make_range(hana::int_c<0>, hana::int_c<2>)
        , hana::make_range(hana::int_c<1>, hana::int_c<1>)
        , hana::make_range(hana::int_c<1>, hana::int_c<2>)
        , hana::make_range(hana::int_c<1>, hana::int_c<3>)
        , hana::make_range(hana::int_c<50>, hana::int_c<60>)

        , hana::make_range(hana::int_c<50>, hana::long_c<60>)
        , hana::make_range(hana::long_c<50>, hana::int_c<60>)
    );

    auto integers = hana::tuple_c<int, 0, 1, 900>;

    hana::test::TestComparable<hana::range_tag>{ranges};
    hana::test::TestFoldable<hana::range_tag>{ranges};
    hana::test::TestIterable<hana::range_tag>{ranges};
    hana::test::TestSearchable<hana::range_tag>{ranges, integers};
}
