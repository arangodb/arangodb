/*
    Copyright 2010 Fabien Castan, Christian Henning
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_EXTENSION_TOOLBOX_CHANNEL_VIEW_HPP_INCLUDED
#define BOOST_GIL_EXTENSION_TOOLBOX_CHANNEL_VIEW_HPP_INCLUDED

////////////////////////////////////////////////////////////////////////////////////////
/// \file channel_view.hpp
/// \brief Helper to generate channel_view type.
/// \author Fabien Castan, Christian Henning \n
///
/// \date   2010 \n
///
////////////////////////////////////////////////////////////////////////////////////////

#include <boost/gil/image_view_factory.hpp>

namespace boost {
namespace gil {

template < typename Channel
         , typename View
         >
struct channel_type_to_index
{
    static const int value = detail::type_to_index< typename color_space_type< View >::type // color (mpl::vector)
                                                  , Channel                                 // channel type
                                                  >::type::value;                           //< index of the channel in the color (mpl::vector)
};

template< typename Channel
        , typename View
        >
struct channel_view_type : public kth_channel_view_type< channel_type_to_index< Channel
                                                                              , View
                                                                              >::value
                                                       , View
                                                       >
{
    static const int index = channel_type_to_index< Channel
                                                  , View
                                                  >::value;
                                                  
    typedef kth_channel_view_type< index
                                 , View
                                 > parent_t;

    typedef typename parent_t::type type;


    static type make( const View& src )
    {
        return parent_t::make( src );
    }
};

/// \ingroup ImageViewTransformationsKthChannel
template< typename Channel
        , typename View
        >
typename channel_view_type< Channel
                          , View
                          >::type channel_view( const View& src )
{
   return channel_view_type< Channel
                           , View
                           >::make( src );
}

} // namespace gil
} // namespace boost

#endif // BOOST_GIL_EXTENSION_TOOLBOX_CHANNEL_VIEW_HPP_INCLUDED
