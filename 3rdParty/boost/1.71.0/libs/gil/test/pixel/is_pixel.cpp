//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/bit_aligned_pixel_reference.hpp>
#include <boost/gil/packed_pixel.hpp>
#include <boost/gil/pixel.hpp>
#include <boost/gil/rgb.hpp>
#include <boost/gil/typedefs.hpp>
#include <boost/gil/concepts/pixel.hpp>

#include <boost/mp11.hpp>

#include "test_fixture.hpp"

namespace gil = boost::gil;
using namespace boost::mp11;

int main()
{
    static_assert(std::is_same
        <
            mp_all_of<gil::test::fixture::pixel_typedefs, gil::is_pixel>,
            std::true_type
        >::value,
        "is_pixel does not yield true for all core pixel types");

    static_assert(std::is_same
        <
            mp_all_of
            <
                mp_transform
                <
                    gil::test::fixture::nested_pixel_type,
                    gil::test::fixture::representative_pixel_types
                >,
                gil::is_pixel
            >,
            std::true_type
        >::value,
        "is_pixel does not yield true for representative pixel types");

    static_assert(std::is_same
        <
            mp_all_of<gil::test::fixture::non_pixels, gil::is_pixel>,
            std::false_type
        >::value,
        "is_pixel yields true for non-pixel type");

    static_assert(std::is_same
        <
            mp_none_of<gil::test::fixture::non_pixels, gil::is_pixel>,
            std::true_type
        >::value,
        "is_pixel yields true for non-pixel type");

    using bgr121_ref_t = gil::bit_aligned_pixel_reference
        <
            std::uint8_t, boost::mpl::vector3_c<int, 1, 2, 1>, gil::bgr_layout_t, true
        >;
    static_assert(gil::is_pixel<bgr121_ref_t>::value,
        "is_pixel does not yield true for bit_aligned_pixel_reference");

    using rgb121_ref_t = gil::bit_aligned_pixel_reference
        <
            std::uint8_t, boost::mpl::vector3_c<int, 1, 2, 1>, gil::rgb_layout_t, true
        >;
    static_assert(gil::is_pixel<bgr121_ref_t>::value,
        "is_pixel does not yield true for bit_aligned_pixel_reference");

    using rgb121_packed_pixel_t = rgb121_ref_t::value_type;
    static_assert(gil::is_pixel<rgb121_packed_pixel_t>::value,
        "is_pixel does not yield true for packed_pixel");
}
