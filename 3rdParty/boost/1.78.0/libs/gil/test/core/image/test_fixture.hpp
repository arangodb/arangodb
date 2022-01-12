//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_TEST_CORE_IMAGE_TEST_FIXTURE_HPP
#define BOOST_GIL_TEST_CORE_IMAGE_TEST_FIXTURE_HPP

#include <boost/gil.hpp>
#include <boost/assert.hpp>

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <random>
#include <tuple>
#include <type_traits>

#include "core/test_fixture.hpp"

namespace boost { namespace gil { namespace test { namespace fixture {

using image_types = std::tuple
<
    gil::gray8_image_t,
    gil::gray16_image_t,
    gil::gray32_image_t,
    gil::bgr8_image_t,
    gil::bgr16_image_t,
    gil::bgr32_image_t,
    gil::rgb8_image_t,
    gil::rgb16_image_t,
    gil::rgb32_image_t,
    gil::rgba8_image_t,
    gil::rgba16_image_t,
    gil::rgba32_image_t
>;

using rgb_interleaved_image_types = std::tuple
<
    gil::bgr8_image_t,
    gil::bgr16_image_t,
    gil::bgr32_image_t,
    gil::rgb8_image_t,
    gil::rgb16_image_t,
    gil::rgb32_image_t,
    gil::rgba8_image_t,
    gil::rgba16_image_t,
    gil::rgba32_image_t
>;

template <typename Image, typename Generator>
auto generate_image(std::ptrdiff_t size_x, std::ptrdiff_t size_y, Generator&& generate) -> Image
{
    using pixel_t = typename Image::value_type;

    Image out(size_x, size_y);
    gil::for_each_pixel(view(out), [&generate](pixel_t& p) {
        gil::static_generate(p, [&generate]() { return generate(); });
    });

    return out;
}

template <typename Image>
auto create_image(std::ptrdiff_t size_x, std::ptrdiff_t size_y, int channel_value) -> Image
{
    using pixel_t = typename Image::value_type;
    using channel_t = typename gil::channel_type<pixel_t>::type;
    static_assert(std::is_integral<channel_t>::value, "channel must be integral type");

    Image out(size_x, size_y);
    gil::for_each_pixel(view(out), [&channel_value](pixel_t& p) {
        gil::static_fill(p, static_cast<channel_t>(channel_value));
    });

    return out;
}

}}}} // namespace boost::gil::test::fixture

#endif
