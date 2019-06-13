//
// Copyright 2012 Christian Henning, Andreas Pokorny, Lubomir Bourdev
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_EXTENSION_TOOLBOX_METAFUNCTIONS_GET_NUM_BITS_HPP
#define BOOST_GIL_EXTENSION_TOOLBOX_METAFUNCTIONS_GET_NUM_BITS_HPP

#include <boost/gil/channel.hpp>

#include <boost/mpl/int.hpp>
#include <boost/mpl/size_t.hpp>
#include <boost/type_traits/is_integral.hpp>
#include <boost/type_traits/is_class.hpp>
#include <boost/utility/enable_if.hpp>

namespace boost{ namespace gil {

/// get_num_bits metafunctions
/// \brief Determines the numbers of bits for the given channel type.

template <typename T, class = void >
struct get_num_bits;

template< typename B, int I, int S, bool M >
struct get_num_bits< packed_channel_reference< B, I, S, M > > : mpl::int_< S >
{};

template< typename B, int I, int S, bool M >
struct get_num_bits< const packed_channel_reference< B, I, S, M > > : mpl::int_< S >
{};

template<typename B, int I, bool M>
struct get_num_bits< packed_dynamic_channel_reference< B, I, M > > : mpl::int_< I >
{};

template<typename B, int I, bool M>
struct get_num_bits< const packed_dynamic_channel_reference< B, I, M > > : mpl::int_< I >
{};

template< int N >
struct get_num_bits< packed_channel_value< N > > : mpl::int_< N >
{};

template< int N >
struct get_num_bits< const packed_channel_value< N > > : mpl::int_< N >
{};

template< typename T >
struct get_num_bits< T
                   , typename enable_if< mpl::and_< is_integral< T >
                                                  , mpl::not_< is_class< T > >
                                                  >
                                       >::type
                   > : mpl::size_t< sizeof(T) * 8 >
{};

} // namespace gil
} // namespace boost

#endif
