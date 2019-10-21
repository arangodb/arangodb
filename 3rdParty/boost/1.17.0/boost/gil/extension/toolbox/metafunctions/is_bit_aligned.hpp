//
// Copyright 2012 Christian Henning, Andreas Pokorny, Lubomir Bourdev
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_EXTENSION_TOOLBOX_METAFUNCTIONS_IS_BIT_ALIGNED_TYPE_HPP
#define BOOST_GIL_EXTENSION_TOOLBOX_METAFUNCTIONS_IS_BIT_ALIGNED_TYPE_HPP

#include <boost/gil/bit_aligned_pixel_reference.hpp>

namespace boost{ namespace gil {

/// is_bit_aligned metafunctions
/// \brief Determines whether the given type is bit_aligned.

template< typename PixelRef >
struct is_bit_aligned : mpl::false_{};

template <typename B, typename C, typename L, bool M>
struct is_bit_aligned<bit_aligned_pixel_reference<B,C,L,M> > : mpl::true_{};

template <typename B, typename C, typename L, bool M>
struct is_bit_aligned<const bit_aligned_pixel_reference<B,C,L,M> > : mpl::true_{};

template <typename B, typename C, typename L>
struct is_bit_aligned<packed_pixel<B,C,L> > : mpl::true_{};

template <typename B, typename C, typename L>
struct is_bit_aligned<const packed_pixel<B,C,L> > : mpl::true_{};

} // namespace gil
} // namespace boost

#endif
