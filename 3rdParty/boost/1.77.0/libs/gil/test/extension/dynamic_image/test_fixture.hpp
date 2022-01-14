//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/dynamic_image/any_image.hpp>

#include <tuple>

namespace boost { namespace gil {

namespace test { namespace fixture {

using dynamic_image = gil::any_image
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

template <typename Image>
struct fill_any_view
{
    using result_type = void;
    using pixel_t = typename Image::value_type;

    fill_any_view(std::initializer_list<int> dst_view_indices, pixel_t pixel_value)
        : dst_view_indices_(dst_view_indices), pixel_value_(pixel_value)
    {}

    template <typename View>
    void operator()(View& /*dst_view*/) { /* sink any other views here */ }

    void operator()(typename Image::view_t& dst_view)
    {
        // sink view of interest here
        for (auto const& i : dst_view_indices_)
            dst_view[i] = pixel_value_;
    }

    std::initializer_list<int> dst_view_indices_;
    pixel_t pixel_value_;
};

}}}} // namespace boost::gil::test::fixture
