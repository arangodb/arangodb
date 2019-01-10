//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/metafunctions/channel_view.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/type_traits/is_same.hpp>

using namespace boost;
using namespace gil;

BOOST_AUTO_TEST_SUITE( toolbox_tests )

BOOST_AUTO_TEST_CASE( channel_view_test )
{
    typedef rgb8_image_t image_t;

    image_t img( 100, 100 );

    typedef kth_channel_view_type< 0, rgb8_view_t::const_t >::type view_t;
    view_t red = kth_channel_view<0>( const_view( img ));

    typedef channel_view_type< red_t, rgb8_view_t::const_t >::type channel_view_t;
    channel_view_t red_ = channel_view< red_t >( const_view( img ));

    BOOST_STATIC_ASSERT(( is_same< kth_channel_view_type< 0, const rgb8_view_t>::type
                                 , channel_view_type< red_t, const rgb8_view_t>::type
                                 >::value
                       ));
}

BOOST_AUTO_TEST_SUITE_END()
