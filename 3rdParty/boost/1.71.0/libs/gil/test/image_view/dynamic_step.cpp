//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/dynamic_step.hpp>
#include <boost/gil/image_view.hpp>
#include <boost/gil/typedefs.hpp>

namespace gil = boost::gil;

template <typename View>
void test()
{
    static_assert(
        gil::view_is_basic
        <
            typename gil::dynamic_x_step_type<View>::type
        >::value, "view does not model HasDynamicXStepTypeConcept");

    static_assert(
        gil::view_is_basic
        <
            typename gil::dynamic_y_step_type<View>::type
        >::value, "view does not model HasDynamicYStepTypeConcept");
}

int main()
{
    test<gil::gray8_view_t>();
    test<gil::gray8_step_view_t>();
    test<gil::gray8c_view_t>();
    test<gil::gray8c_step_view_t>();
    test<gil::gray16_view_t>();
    test<gil::gray16_step_view_t>();
    test<gil::gray16c_view_t>();
    test<gil::gray16c_step_view_t>();
    test<gil::gray32_view_t>();
    test<gil::gray32_step_view_t>();
    test<gil::gray32c_view_t>();
    test<gil::gray32c_step_view_t>();
    test<gil::gray32f_view_t>();
    test<gil::gray32f_step_view_t>();

    test<gil::abgr8_view_t>();
    test<gil::abgr8_step_view_t>();
    test<gil::abgr16_view_t>();
    test<gil::abgr16_step_view_t>();
    test<gil::abgr32_view_t>();
    test<gil::abgr32_step_view_t>();
    test<gil::abgr32f_view_t>();
    test<gil::abgr32f_step_view_t>();

    test<gil::argb8_view_t>();
    test<gil::argb8_step_view_t>();
    test<gil::argb16_view_t>();
    test<gil::argb16_step_view_t>();
    test<gil::argb32_view_t>();
    test<gil::argb32_step_view_t>();
    test<gil::argb32f_view_t>();
    test<gil::argb32f_step_view_t>();

    test<gil::bgr8_view_t>();
    test<gil::bgr8_step_view_t>();
    test<gil::bgr16_view_t>();
    test<gil::bgr16_step_view_t>();
    test<gil::bgr32_view_t>();
    test<gil::bgr32_step_view_t>();
    test<gil::bgr32f_view_t>();
    test<gil::bgr32f_step_view_t>();

    test<gil::bgra8_view_t>();
    test<gil::bgra8_step_view_t>();
    test<gil::bgra16_view_t>();
    test<gil::bgra16_step_view_t>();
    test<gil::bgra32_view_t>();
    test<gil::bgra32_step_view_t>();
    test<gil::bgra32f_view_t>();
    test<gil::bgra32f_step_view_t>();

    test<gil::rgb8_view_t>();
    test<gil::rgb8_step_view_t>();
    test<gil::rgb8_planar_view_t>();
    test<gil::rgb8_planar_step_view_t>();
    test<gil::rgb16_view_t>();
    test<gil::rgb16_step_view_t>();
    test<gil::rgb16_planar_view_t>();
    test<gil::rgb16_planar_step_view_t>();
    test<gil::rgb32_view_t>();
    test<gil::rgb32_step_view_t>();
    test<gil::rgb32_planar_view_t>();
    test<gil::rgb32_planar_step_view_t>();
    test<gil::rgb32f_view_t>();
    test<gil::rgb32f_step_view_t>();
    test<gil::rgb32f_planar_view_t>();
    test<gil::rgb32f_planar_step_view_t>();

    test<gil::rgba8_view_t>();
    test<gil::rgba8_step_view_t>();
    test<gil::rgba8_planar_view_t>();
    test<gil::rgba8_planar_step_view_t>();
    test<gil::rgba16_view_t>();
    test<gil::rgba16_step_view_t>();
    test<gil::rgba16_planar_view_t>();
    test<gil::rgba16_planar_step_view_t>();
    test<gil::rgba32_view_t>();
    test<gil::rgba32_step_view_t>();
    test<gil::rgba32_planar_view_t>();
    test<gil::rgba32_planar_step_view_t>();
    test<gil::rgba32f_view_t>();
    test<gil::rgba32f_step_view_t>();
    test<gil::rgba32f_planar_view_t>();
    test<gil::rgba32f_planar_step_view_t>();

    test<gil::cmyk8_view_t>();
    test<gil::cmyk8_step_view_t>();
    test<gil::cmyk8_planar_view_t>();
    test<gil::cmyk8_planar_step_view_t>();
    test<gil::cmyk16_view_t>();
    test<gil::cmyk16_step_view_t>();
    test<gil::cmyk16_planar_view_t>();
    test<gil::cmyk16_planar_step_view_t>();
    test<gil::cmyk32_view_t>();
    test<gil::cmyk32_step_view_t>();
    test<gil::cmyk32_planar_view_t>();
    test<gil::cmyk32_planar_step_view_t>();
    test<gil::cmyk32f_view_t>();
    test<gil::cmyk32f_step_view_t>();
    test<gil::cmyk32f_planar_view_t>();
    test<gil::cmyk32f_planar_step_view_t>();
}
