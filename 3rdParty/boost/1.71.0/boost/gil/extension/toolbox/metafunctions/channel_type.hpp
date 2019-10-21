//
// Copyright 2012 Christian Henning, Andreas Pokorny, Lubomir Bourdev
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_EXTENSION_TOOLBOX_METAFUNCTIONS_CHANNEL_TYPE_HPP
#define BOOST_GIL_EXTENSION_TOOLBOX_METAFUNCTIONS_CHANNEL_TYPE_HPP

#include <boost/gil/extension/toolbox/dynamic_images.hpp>
#include <boost/gil/extension/toolbox/metafunctions/get_num_bits.hpp>
#include <boost/gil/extension/toolbox/metafunctions/is_homogeneous.hpp>

#include <boost/gil/bit_aligned_pixel_reference.hpp>
#include <boost/gil/channel.hpp>

#include <boost/mpl/at.hpp>
#include <boost/utility/enable_if.hpp> // boost::lazy_enable_if

namespace boost{ namespace gil {

/// channel_type metafunction
/// \brief Generates the channel type for

template <typename B, typename C, typename L, bool M>
struct gen_chan_ref
{
    using type = packed_dynamic_channel_reference
        <
            B,
            mpl::at_c<C, 0>::type::value,
            M
        >;
};

//! This implementation works for bit_algined_pixel_reference
//! with a homogeneous channel layout.
//! The result type will be a packed_dynamic_channel_reference, since the
//! offset info will be missing.

// bit_aligned_pixel_reference
template <typename B, typename C, typename L, bool M>
struct channel_type< bit_aligned_pixel_reference<B,C,L,M> >
    : lazy_enable_if< is_homogeneous< bit_aligned_pixel_reference< B, C, L, M > >
                    , gen_chan_ref< B, C, L, M >
                    > {};

template <typename B, typename C, typename L, bool M>
struct channel_type<const bit_aligned_pixel_reference<B,C,L,M> >
    : lazy_enable_if< is_homogeneous< bit_aligned_pixel_reference< B, C, L, M > >
                    , gen_chan_ref< B, C, L, M >
                    > {};

template <typename B, typename C, typename L>
struct gen_chan_ref_p
{
    using type = packed_dynamic_channel_reference
        <
            B,
            get_num_bits<typename mpl::at_c<C, 0>::type>::value,
            true
        >;
};

// packed_pixel
template < typename BitField
         , typename ChannelRefVec
         , typename Layout
         >
struct channel_type< packed_pixel< BitField
                                 , ChannelRefVec
                                 , Layout
                                 >
                   > : lazy_enable_if< is_homogeneous< packed_pixel< BitField
                                                                   , ChannelRefVec
                                                                   , Layout
                                                                   >
                                                     >
                                     , gen_chan_ref_p< BitField
                                                     , ChannelRefVec
                                                     , Layout
                                                     >
                                     > {};

template <typename B, typename C, typename L>
struct channel_type< const packed_pixel< B, C, L > >
    : lazy_enable_if< is_homogeneous<packed_pixel< B, C, L > >
                    , gen_chan_ref_p< B, C, L >
                    >
{};

template<>
struct channel_type< any_image_pixel_t >
{
    using type = any_image_channel_t;
};

} // namespace gil
} // namespace boost

#endif
