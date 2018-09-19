/*
    Copyright 2009 Christian Henning
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_EXTENSION_IO_RAW_DETAIL_IS_ALLOWED_HPP
#define BOOST_GIL_EXTENSION_IO_RAW_DETAIL_IS_ALLOWED_HPP

////////////////////////////////////////////////////////////////////////////////////////
/// \file
/// \brief
/// \author Olivier Tournaire \n
///
/// \date 2011 \n
///
////////////////////////////////////////////////////////////////////////////////////////

#include <boost/gil/io/base.hpp>
#include <iostream>

namespace boost { namespace gil { namespace detail {

template< typename View >
bool is_allowed( const image_read_info< raw_tag >& info
               , mpl::true_   // is read_and_no_convert
               )
{
    typedef typename get_pixel_type< View >::type pixel_t;
    typedef typename num_channels< pixel_t >::value_type num_channel_t;
    typedef typename channel_traits< typename element_type< typename View::value_type >::type >::value_type channel_t;

    const num_channel_t dst_samples_per_pixel = num_channels< pixel_t >::value;
    const unsigned int  dst_bits_per_pixel    = detail::unsigned_integral_num_bits< channel_t >::value;
    const bool          is_type_signed        = boost::is_signed< channel_t >::value;

    return( dst_samples_per_pixel == info._samples_per_pixel &&
            dst_bits_per_pixel    == static_cast<unsigned int>(info._bits_per_pixel) &&
            !is_type_signed );
}

template< typename View >
bool is_allowed( const image_read_info< raw_tag >& /* info */
               , mpl::false_  // is read_and_convert
               )
{
    return true;
}

} // namespace detail
} // namespace gil
} // namespace boost

#endif
