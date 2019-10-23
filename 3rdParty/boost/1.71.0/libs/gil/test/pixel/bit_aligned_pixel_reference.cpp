//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/bit_aligned_pixel_reference.hpp>
#include <boost/gil/packed_pixel.hpp>
#include <boost/gil/rgb.hpp>
#include <boost/mpl/vector_c.hpp>
namespace gil = boost::gil;
namespace mpl = boost::mpl;

int main()
{
    using bgr121_ref_t = gil::bit_aligned_pixel_reference
        <
            std::uint8_t, mpl::vector3_c<int, 1, 2, 1>, gil::bgr_layout_t, true
        >;

    static_assert(bgr121_ref_t::bit_size == 4,
        "bit size should be 4");

    static_assert(std::is_same<bgr121_ref_t::bitfield_t, std::uint8_t>::value,
        "bit field type should be std::uint8_t");

    static_assert(std::is_same<bgr121_ref_t::layout_t, gil::bgr_layout_t>::value,
        "layout type should be bgr");

    static_assert(std::is_same<decltype(bgr121_ref_t::is_mutable), bool const>::value &&
        bgr121_ref_t::is_mutable,
        "is_mutable should be boolean");

    using packed_pixel_t = gil::packed_pixel
        <
            std::uint8_t,
            typename gil::detail::packed_channel_references_vector_type
            <
                std::uint8_t, mpl::vector3_c<int, 1, 2, 1>
            >::type,
            gil::bgr_layout_t
        >;
    static_assert(std::is_same<bgr121_ref_t::value_type, packed_pixel_t >::value,
        "value_type should be specialization of packed_pixel");
}
