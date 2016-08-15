// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/detail/canonical_constant.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/comparable.hpp>
#include <laws/constant.hpp>
#include <laws/euclidean_ring.hpp>
#include <laws/group.hpp>
#include <laws/logical.hpp>
#include <laws/monoid.hpp>
#include <laws/orderable.hpp>
#include <laws/ring.hpp>
namespace hana = boost::hana;


template <typename T, T v>
struct canonical {
    static constexpr T value = v;
    using hana_tag = hana::detail::CanonicalConstant<T>;
};

int main() {
    auto ints = hana::make_tuple(
        canonical<int, -10>{}, canonical<int, -2>{}, canonical<int, 0>{},
        canonical<int, 1>{}, canonical<int, 3>{}, canonical<int, 4>{}
    );

    auto bools = hana::make_tuple(canonical<bool, true>{}, canonical<bool, false>{});

    // Constant
    hana::test::TestConstant<hana::detail::CanonicalConstant<int>>{ints, hana::tuple_t<int, long, long long>};
    hana::test::TestConstant<hana::detail::CanonicalConstant<bool>>{bools, hana::tuple_t<bool>};

    // Monoid, Group, Ring, EuclideanRing
    hana::test::TestMonoid<hana::detail::CanonicalConstant<int>>{ints};
    hana::test::TestGroup<hana::detail::CanonicalConstant<int>>{ints};
    hana::test::TestRing<hana::detail::CanonicalConstant<int>>{ints};
    hana::test::TestEuclideanRing<hana::detail::CanonicalConstant<int>>{ints};

    // Logical
    {
        auto ints = hana::make_tuple(
            canonical<int, -2>{}, canonical<int, 0>{},
            canonical<int, 1>{}, canonical<int, 3>{}
        );
        hana::test::TestLogical<hana::detail::CanonicalConstant<int>>{ints};
        hana::test::TestLogical<hana::detail::CanonicalConstant<bool>>{bools};
    }

    // Comparable and Orderable
    hana::test::TestComparable<hana::detail::CanonicalConstant<int>>{ints};
    hana::test::TestOrderable<hana::detail::CanonicalConstant<int>>{ints};
}
