/*
    Copyright 2008 Christian Henning, Lubomir Bourdev
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_EXTENSION_IO_TIFF_DETAIL_IS_ALLOWED_HPP
#define BOOST_GIL_EXTENSION_IO_TIFF_DETAIL_IS_ALLOWED_HPP

////////////////////////////////////////////////////////////////////////////////////////
/// \file
/// \brief
/// \author Christian Henning \n
///
/// \date 2008 \n
///
////////////////////////////////////////////////////////////////////////////////////////

#include <boost/mpl/for_each.hpp>

#include <boost/gil/io/base.hpp>

namespace boost { namespace gil { namespace detail {

typedef std::vector< tiff_bits_per_sample::type > channel_sizes_t;

template< typename View, typename Channel, typename Enable = void > struct Format_Type {};

// is_bit_aligned< View >
template< typename View, typename Channel > struct Format_Type< View
                                                              , Channel
                                                              , typename boost::enable_if< typename is_bit_aligned< typename get_pixel_type< View >::type >::type >::type
                                                              >
{
    static const int value = SAMPLEFORMAT_UINT;
};

// is_not_bit_aligned< View > && is_unsigned< Channel >
template< typename View, typename Channel > struct Format_Type< View
                                                              , Channel
                                                              , typename boost::enable_if< mpl::and_< mpl::not_< typename is_bit_aligned< typename get_pixel_type< View >::type >::type >
                                                                                                    , is_unsigned< Channel > 
                                                                                                    >
                                                                                         >::type
                                                              >
{
    static const int value = SAMPLEFORMAT_UINT;
};

// is_not_bit_aligned< View > && is_signed< Channel >
template< typename View, typename Channel > struct Format_Type< View
                                                              , Channel
                                                              , typename boost::enable_if< mpl::and_< mpl::not_< typename is_bit_aligned< typename get_pixel_type< View >::type >::type >
                                                                                                    , is_signed< Channel > 
                                                                                                    >
                                                                                         >::type
                                                              >
{
    static const int value = SAMPLEFORMAT_INT;
};

// is_not_bit_aligned< View > && is_floating_point< Channel >
template< typename View, typename Channel > struct Format_Type< View
                                                              , Channel
                                                              , typename boost::enable_if< mpl::and_< mpl::not_< typename is_bit_aligned< typename get_pixel_type< View >::type >::type >
                                                                                                    , is_floating_point< Channel > 
                                                                                                    >
                                                                                         >::type
                                                              >
{
    static const int value = SAMPLEFORMAT_IEEEFP;
};

//template< typename Channel >
//int format_value( mpl::true_ ) // is_bit_aligned
//{
//    return SAMPLEFORMAT_UINT;
//}
//
//template< typename Channel >
//int format_value( mpl::false_ ) // is_bit_aligned
//{
//    if( is_unsigned< Channel >::value )
//    {
//        return SAMPLEFORMAT_UINT;
//    }
//
//    if( is_signed< Channel >::value )
//    {
//        return SAMPLEFORMAT_INT;
//    }
//
//    else if( is_floating_point< Channel >::value )
//    {
//        return SAMPLEFORMAT_IEEEFP;
//    }
//
//    io_error( "Unkown channel format." );
//}

// The following two functions look the same but are different since one is using
// a pixel_t as template parameter whereas the other is using reference_t.
template< typename View >
bool compare_channel_sizes( const channel_sizes_t& channel_sizes // in bits
                          , mpl::false_                          // is_bit_aligned
                          , mpl::true_                           // is_homogeneous
                          )
{
    typedef typename View::value_type pixel_t;
    typedef typename channel_traits<
                typename element_type< pixel_t >::type >::value_type channel_t;

    unsigned int s = detail::unsigned_integral_num_bits< channel_t >::value;

    return ( s == channel_sizes[0] );
}


template< typename View >
bool compare_channel_sizes( const channel_sizes_t& channel_sizes // in bits
                          , mpl::true_                           // is_bit_aligned
                          , mpl::true_                           // is_homogeneous
                          )
{
    typedef typename View::reference ref_t;

    typedef typename channel_traits< typename element_type< ref_t >::type >::value_type channel_t;
    channel_t c;

    unsigned int s = detail::unsigned_integral_num_bits< channel_t >::value;

    return ( s == channel_sizes[0] );
}

struct compare_channel_sizes_fn
{
    compare_channel_sizes_fn( uint16_t* a )
    : _a( a )
    , _b( true )
    {}

    template< typename ChannelSize >
    void operator()( ChannelSize x)
    {
        if( x != *_a++ )
        {
            _b = false;
        }
    }

    uint16_t* _a;
    bool _b;
};

template< typename T >
struct channel_sizes_type {};

template< typename B, typename C, typename L, bool M >
struct channel_sizes_type< bit_aligned_pixel_reference< B, C, L, M > > { typedef C type; };

template< typename B, typename C, typename L, bool M >
struct channel_sizes_type< const bit_aligned_pixel_reference< B, C, L, M > > { typedef C type; };

template< typename View >
bool compare_channel_sizes( channel_sizes_t& channel_sizes // in bits
                          , mpl::true_                     // is_bit_aligned
                          , mpl::false_                    // is_homogeneous
                          )
{
    // loop through all channels and compare

    typedef typename View::reference ref_t;
    typedef typename channel_sizes_type< ref_t >::type cs_t;

    compare_channel_sizes_fn fn( &channel_sizes.front() );
    mpl::for_each< cs_t >( fn );

    return fn._b;
}

template< typename View >
bool is_allowed( const image_read_info< tiff_tag >& info
               , mpl::true_ // is read_and_no_convert
               )
{
    channel_sizes_t channel_sizes( info._samples_per_pixel
                                 , info._bits_per_sample
                                 );

    typedef typename get_pixel_type< View >::type pixel_t;
    typedef typename channel_traits<
                typename element_type< pixel_t >::type >::value_type channel_t;

    typedef typename num_channels< pixel_t >::value_type num_channel_t;

    const num_channel_t dst_samples_per_pixel = num_channels< pixel_t >::value;

    //const num_channel_t dst_sample_format     = format_value< channel_t >( typename is_bit_aligned< pixel_t >::type() );
    const num_channel_t dst_sample_format     = Format_Type<View, channel_t>::value;


    return (  dst_samples_per_pixel == info._samples_per_pixel
           && compare_channel_sizes< View >( channel_sizes
                                           , typename is_bit_aligned< pixel_t >::type()
                                           , typename is_homogeneous< pixel_t >::type()
                                           )
           && dst_sample_format == info._sample_format
           );
}

template< typename View >
bool is_allowed( const image_read_info< tiff_tag >& /* info */
               , mpl::false_ // is read_and_no_convert
               )
{
    return true;
}

} // namespace detail
} // namespace gil
} // namespace boost

#endif
