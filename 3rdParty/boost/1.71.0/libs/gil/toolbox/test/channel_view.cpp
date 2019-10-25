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

#include <type_traits>

namespace bg = boost::gil;

BOOST_AUTO_TEST_SUITE(toolbox_tests)

BOOST_AUTO_TEST_CASE(channel_view_test)
{
    using image_t = bg::rgb8_image_t;
    image_t img(100, 100);

    using kth_channel_view_t
        = bg::kth_channel_view_type<0, bg::rgb8_view_t::const_t>::type;
    using channel_view_t
        = bg::channel_view_type<bg::red_t, bg::rgb8_view_t::const_t>::type;

    static_assert(std::is_same
        <
            kth_channel_view_t,
            channel_view_t
        >::value,
        "");

    kth_channel_view_t const kth0 = bg::kth_channel_view<0>(bg::const_view(img));
    BOOST_TEST(kth0.num_channels() == 1u);

    channel_view_t const red = bg::channel_view<bg::red_t>(bg::const_view(img));
    BOOST_TEST(red.num_channels() == 1u);
}

BOOST_AUTO_TEST_SUITE_END()
