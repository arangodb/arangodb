//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/channel.hpp>
#include <boost/gil/packed_pixel.hpp>
#include <boost/gil/gray.hpp>
#include <boost/gil/rgb.hpp>

#include <boost/core/typeinfo.hpp>
#include <boost/mpl/at.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/size.hpp>
#include <boost/mpl/vector_c.hpp>

#define BOOST_TEST_MODULE test_channel_traits
#include "unit_test.hpp"

namespace gil = boost::gil;
namespace mpl = boost::mpl;

namespace boost { namespace gil {

template <typename BitField, typename ChannelRefs, typename Layout>
std::ostream& operator<<(std::ostream& os, gil::packed_pixel<BitField, ChannelRefs, Layout> const& p)
{
    os << "packed_pixel<"
       << "BitField=" << boost::core::demangled_name(typeid(BitField))
       << ", ChannelRefs="  << boost::core::demangled_name(typeid(ChannelRefs))
       << ", Layout=" << boost::core::demangled_name(typeid(Layout))
        << ">(" << (std::uint64_t)p._bitfield << ")";
    return os;
}

}} // namespace boost::gil

using packed_channel_references_3 = typename gil::detail::packed_channel_references_vector_type
<
    std::uint8_t,
    mpl::vector1_c<int, 3>
>::type;

using packed_pixel_gray3 = gil::packed_pixel
<
    std::uint8_t,
    packed_channel_references_3,
    gil::gray_layout_t
>;

BOOST_AUTO_TEST_CASE(packed_pixel_gray3_definition)
{
    // Verify packed_pixel members

    static_assert(std::is_same<packed_pixel_gray3::layout_t, gil::gray_layout_t>::value,
        "layout should be bgr");

    static_assert(std::is_same<packed_pixel_gray3::value_type, packed_pixel_gray3>::value,
        "value_type member should be of the same type as the packed_pixel specialization");

    static_assert(std::is_reference<packed_pixel_gray3::reference>::value,
        "reference member should be a reference");

    static_assert(std::is_reference<packed_pixel_gray3::const_reference>::value,
        "const_reference member should be a reference");

    static_assert(std::is_same<decltype(packed_pixel_gray3::is_mutable), bool const>::value &&
        packed_pixel_gray3::is_mutable,
        "is_mutable should be boolean");

    // Verify metafunctions

    using channel_references_t = packed_channel_references_3::type;

    static_assert(mpl::size<channel_references_t>::value == 1,
        "packed_channel_references_vector_type should define one reference to channel start bits");

    using channel1_ref_t = mpl::at_c<channel_references_t, 0>::type;
    static_assert(channel1_ref_t::num_bits == 3,
        "1st channel of gray3 pixel should be of 3-bit size");

    static_assert(std::is_same
        <
            channel1_ref_t,
            gil::packed_channel_reference<std::uint8_t, 0, 3, true> const
        >::value,
        "1st element of packed_channel_references_vector should be packed_channel_reference of 1st channel");

    // double check intermediate metafunction packed_channel_reference_type
    static_assert(std::is_same
        <
            gil::detail::packed_channel_reference_type<std::uint8_t, mpl::int_<0>, mpl::int_<3>>::type,
            channel1_ref_t
        >::value,
        "packed_channel_reference_type should return packed_channel_reference");
    static_assert(std::is_same
        <
            gil::detail::packed_channel_reference_type<std::uint8_t, mpl::int_<0>, mpl::int_<3>>::type,
            gil::packed_channel_reference<std::uint8_t, 0, 3, true> const
        >::value,
        "packed_channel_reference_type should return packed_channel_reference");
}

BOOST_AUTO_TEST_CASE(packed_pixel_gray3_assignment)
{
    packed_pixel_gray3 p1{int{5}};
    packed_pixel_gray3 p2;
    p2 = p1;
    BOOST_TEST(p1._bitfield == p2._bitfield);
}

BOOST_AUTO_TEST_CASE(packed_pixel_gray_equality)
{
    packed_pixel_gray3 p1{int{5}};
    packed_pixel_gray3 p2{int{5}};
    BOOST_TEST(p1 == p2);

    packed_pixel_gray3 p3{int{3}};
    BOOST_TEST(p2 != p3);
}

BOOST_AUTO_TEST_CASE(packed_pixel_gray3_assignment_gray_channel)
{
    packed_pixel_gray3 p1{0};
    p1 = int{5};
    BOOST_TEST(p1._bitfield == int{5});
}

BOOST_AUTO_TEST_CASE(packed_pixel_gray_equality_gray_channel)
{
    packed_pixel_gray3 p1{int{3}};
    BOOST_TEST(p1 == int{3});
}

using packed_channel_references_121 = typename gil::detail::packed_channel_references_vector_type
<
    std::uint8_t,
    mpl::vector3_c<int, 1, 2, 1>
>::type;

using packed_pixel_bgr121 = gil::packed_pixel
<
    std::uint8_t,
    packed_channel_references_121,
    gil::bgr_layout_t
>;

BOOST_AUTO_TEST_CASE(packed_pixel_bgr121_definition)
{
    // Verify packed_pixel members

    static_assert(std::is_same<packed_pixel_bgr121::layout_t, gil::bgr_layout_t>::value,
        "layout should be bgr");

    static_assert(std::is_same<packed_pixel_bgr121::value_type, packed_pixel_bgr121>::value,
        "value_type member should be of the same type as the packed_pixel specialization");

    static_assert(std::is_reference<packed_pixel_bgr121::reference>::value,
        "reference member should be a reference");

    static_assert(std::is_reference<packed_pixel_bgr121::const_reference>::value,
        "const_reference member should be a reference");

    static_assert(std::is_same<decltype(packed_pixel_bgr121::is_mutable), bool const>::value &&
        packed_pixel_bgr121::is_mutable,
        "is_mutable should be boolean");

    // Verify metafunctions

    using channel_references_t = packed_channel_references_121::type;

    static_assert(mpl::size<channel_references_t>::value == 3,
        "packed_channel_references_vector_type should define three references to channel start bits");

    using channel1_ref_t = mpl::at_c<channel_references_t, 0>::type;
    static_assert(channel1_ref_t::num_bits == 1,
        "1st channel of bgr121 pixel should be of 1-bit size");

    using channel2_ref_t = mpl::at_c<channel_references_t, 1>::type;
    static_assert(channel2_ref_t::num_bits == 2,
        "2nd channel of bgr121 pixel should be of 2-bit size");

    using channel3_ref_t = mpl::at_c<channel_references_t, 2>::type;
    static_assert(channel3_ref_t::num_bits == 1,
        "3rd channel of bgr121 pixel should be of 1-bit size");

    static_assert(std::is_same
        <
            channel1_ref_t,
            gil::packed_channel_reference<std::uint8_t, 0, 1, true> const
        >::value,
        "1st element of packed_channel_references_vector should be packed_channel_reference of 1st channel");

    static_assert(std::is_same
        <
            channel2_ref_t,
            gil::packed_channel_reference<std::uint8_t, 1, 2, true> const
        >::value,
        "2nd element of packed_channel_references_vector should be packed_channel_reference of 2nd channel");

    static_assert(std::is_same
        <
            channel3_ref_t,
            gil::packed_channel_reference<std::uint8_t, 3, 1, true> const
        >::value,
        "3rd element of packed_channel_references_vector should be packed_channel_reference of 3rd channel");

    // double check intermediate metafunction packed_channel_reference_type
    static_assert(std::is_same
        <
            gil::detail::packed_channel_reference_type<std::uint8_t, mpl::int_<0>, mpl::int_<1>>::type,
            channel1_ref_t
        >::value,
        "packed_channel_reference_type should return packed_channel_reference");
    static_assert(std::is_same
        <
            gil::detail::packed_channel_reference_type<std::uint8_t, mpl::int_<0>, mpl::int_<1>>::type,
            gil::packed_channel_reference<std::uint8_t, 0, 1, true> const
        >::value,
        "packed_channel_reference_type should return packed_channel_reference");
}

BOOST_AUTO_TEST_CASE(packed_pixel_bgr121_assignment)
{
    packed_pixel_bgr121 p1{0, 3, 1};
    packed_pixel_bgr121 p2;
    p2 = p1;
    BOOST_TEST(p1._bitfield == p2._bitfield);
}

BOOST_AUTO_TEST_CASE(packed_pixel_bgr121_equality)
{
    packed_pixel_bgr121 p1{1, 3, 0};
    packed_pixel_bgr121 p2{1, 3, 0};
    BOOST_TEST(p1 == p2);

    packed_pixel_bgr121 p3{0, 3, 1};
    BOOST_TEST(p2 != p3);
}

using packed_channel_references_535 = typename gil::detail::packed_channel_references_vector_type
<
    std::uint16_t,
    mpl::vector3_c<int, 5, 3, 5>
>::type;

using packed_pixel_rgb535 = gil::packed_pixel
<
    std::uint16_t,
    packed_channel_references_535,
    gil::rgb_layout_t
>;

BOOST_AUTO_TEST_CASE(packed_pixel_rgb535_definition)
{
    // Verify packed_pixel members

    static_assert(std::is_same<packed_pixel_rgb535::layout_t, gil::rgb_layout_t>::value,
        "layout should be bgr");

    static_assert(std::is_same<packed_pixel_rgb535::value_type, packed_pixel_rgb535>::value,
        "value_type member should be of the same type as the packed_pixel specialization");

    static_assert(std::is_reference<packed_pixel_rgb535::reference>::value,
        "reference member should be a reference");

    static_assert(std::is_reference<packed_pixel_rgb535::const_reference>::value,
        "const_reference member should be a reference");

    static_assert(std::is_same<decltype(packed_pixel_rgb535::is_mutable), bool const>::value &&
        packed_pixel_rgb535::is_mutable,
        "is_mutable should be boolean");

    // Verify metafunctions

    using channel_references_t = packed_channel_references_535::type;

    static_assert(mpl::size<channel_references_t>::value == 3,
        "packed_channel_references_vector_type should define three references to channel start bits");

    using channel1_ref_t = mpl::at_c<channel_references_t, 0>::type;
    static_assert(channel1_ref_t::num_bits == 5,
        "1st channel of rgb535 pixel should be of 5-bit size");

    using channel2_ref_t = mpl::at_c<channel_references_t, 1>::type;
    static_assert(channel2_ref_t::num_bits == 3,
        "2nd channel of rgb535 pixel should be of 3-bit size");

    using channel3_ref_t = mpl::at_c<channel_references_t, 2>::type;
    static_assert(channel3_ref_t::num_bits == 5,
        "3rd channel of rgb535 pixel should be of 5-bit size");

    static_assert(std::is_same
        <
            channel1_ref_t,
            gil::packed_channel_reference<std::uint16_t, 0, 5, true> const
        >::value,
        "1st element of packed_channel_references_vector should be packed_channel_reference of 1st channel");

    static_assert(std::is_same
        <
            channel2_ref_t,
            gil::packed_channel_reference<std::uint16_t, 5, 3, true> const
        >::value,
        "2nd element of packed_channel_references_vector should be packed_channel_reference of 2nd channel");

    static_assert(std::is_same
        <
            channel3_ref_t,
            gil::packed_channel_reference<std::uint16_t, 8, 5, true> const
        >::value,
        "3rd element of packed_channel_references_vector should be packed_channel_reference of 3rd channel");

    // double check intermediate metafunction packed_channel_reference_type
    static_assert(std::is_same
        <
            gil::detail::packed_channel_reference_type<std::uint16_t, mpl::int_<0>, mpl::int_<5>>::type,
            channel1_ref_t
        >::value,
        "packed_channel_reference_type should return packed_channel_reference");
    static_assert(std::is_same
        <
            gil::detail::packed_channel_reference_type<std::uint16_t, mpl::int_<0>, mpl::int_<5>>::type,
            gil::packed_channel_reference<std::uint16_t, 0, 5, true> const
        >::value,
        "packed_channel_reference_type should return packed_channel_reference");
}

BOOST_AUTO_TEST_CASE(packed_pixel_rgb535_assignment)
{
    packed_pixel_rgb535 p1{31, 7, 31};
    packed_pixel_rgb535 p2;
    p2 = p1;
    BOOST_TEST(p1._bitfield == p2._bitfield);
}

BOOST_AUTO_TEST_CASE(packed_pixel_rgb535_equality)
{
    packed_pixel_rgb535 p1{7, 3, 7};
    packed_pixel_rgb535 p2{7, 3, 7};
    BOOST_TEST(p1 == p2);

    packed_pixel_rgb535 p3{7, 7, 7};
    BOOST_TEST(p2 != p3);
}
