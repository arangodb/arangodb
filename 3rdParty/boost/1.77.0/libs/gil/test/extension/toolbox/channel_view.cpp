//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/metafunctions/channel_view.hpp>

#include <boost/core/lightweight_test.hpp>

#include <type_traits>

namespace gil = boost::gil;

void test_channel_view()
{
    using kth_channel_view_t
        = gil::kth_channel_view_type<0, gil::rgb8_view_t::const_t>::type;
    using channel_view_t
        = gil::channel_view_type<gil::red_t, gil::rgb8_view_t::const_t>::type;

    static_assert(std::is_same
        <
            kth_channel_view_t,
            channel_view_t
        >::value,
        "");

    gil::rgb8_image_t img(100, 100);
    kth_channel_view_t const kth0 = gil::kth_channel_view<0>(gil::const_view(img));
    BOOST_TEST_EQ(kth0.num_channels(), 1u);

    channel_view_t const red = gil::channel_view<gil::red_t>(gil::const_view(img));
    BOOST_TEST_EQ(red.num_channels(), 1u);
}

int main()
{
    test_channel_view();

    return boost::report_errors();
}
