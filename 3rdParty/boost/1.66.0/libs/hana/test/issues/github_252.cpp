// Copyright Sergey Nizovtsev 2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/core/is_a.hpp>
#include <boost/hana/functional/partial.hpp>
#include <boost/hana/product.hpp>
#include <boost/hana/range.hpp>
#include <boost/hana/traits.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>

namespace hana = boost::hana;

int main() {
    constexpr auto type = hana::type_c<int[2][3][4]>;

    BOOST_HANA_CONSTANT_CHECK(
        hana::is_an<hana::integral_constant_tag<size_t>>(
            hana::traits::extent(type, hana::uint_c<1>)
        )
    );

    // Check that we can multiple extents in size_t's ring
    hana::product<hana::integral_constant_tag<size_t>>(
        hana::transform(
            hana::to_tuple(
                hana::make_range(
                    hana::size_c<0>,
                    hana::traits::rank(type)
                )
            ),
            hana::partial(hana::traits::extent, type)
        )
    );
}
