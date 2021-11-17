// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/symmetric_difference.hpp>

namespace hana = boost::hana;


constexpr auto m1 = hana::make_map(
    hana::make_pair(hana::type_c<int>, 1),
    hana::make_pair(hana::type_c<bool>, hana::true_c)
);

constexpr auto m2 = hana::make_map(
    hana::make_pair(hana::type_c<float>, 1.0),
    hana::make_pair(hana::type_c<long long>, 2LL),
    hana::make_pair(hana::type_c<int>, 3)
);

constexpr auto result_m = hana::make_map(
    hana::make_pair(hana::type_c<bool>, hana::true_c),
    hana::make_pair(hana::type_c<float>, 1.0),
    hana::make_pair(hana::type_c<long long>, 2LL)
);

int main() {
    BOOST_HANA_RUNTIME_CHECK(
            hana::symmetric_difference(m1, m2) == result_m
    );
}
