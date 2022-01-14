//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/dynamic_step.hpp>
#include <boost/gil/color_base.hpp> // kth_element_type
#include <boost/gil/pixel.hpp> // kth_element_type
#include <boost/gil/locator.hpp>
#include <boost/gil/pixel_iterator.hpp>
#include <boost/gil/typedefs.hpp>

#include <type_traits>

namespace gil = boost::gil;

template <typename Locator>
void test_locator_from_iterator()
{
    // The `memory_based_2d_locator` for X-step is calculated based on adapted iterator
    // Only verify `type` member is available (i.e. specialization defined).
    static_assert(std::is_class
        <
            typename gil::dynamic_x_step_type<Locator>::type
        >::value, "");

    static_assert(std::is_same
        <
            Locator,
            typename gil::dynamic_y_step_type<Locator>::type
        >::value, "locator does not model HasDynamicYStepTypeConcept");
}

int main()
{
    test_locator_from_iterator<gil::gray8_loc_t>();
    test_locator_from_iterator<gil::gray8c_loc_t>();
    test_locator_from_iterator<gil::gray16_loc_t>();
    test_locator_from_iterator<gil::gray16c_loc_t>();
    test_locator_from_iterator<gil::gray32_loc_t>();
    test_locator_from_iterator<gil::gray32c_loc_t>();
    test_locator_from_iterator<gil::gray32f_loc_t>();

    test_locator_from_iterator<gil::abgr8_loc_t>();
    test_locator_from_iterator<gil::abgr16_loc_t>();
    test_locator_from_iterator<gil::abgr32_loc_t>();
    test_locator_from_iterator<gil::abgr32f_loc_t>();

    test_locator_from_iterator<gil::argb8_loc_t>();
    test_locator_from_iterator<gil::argb16_loc_t>();
    test_locator_from_iterator<gil::argb32_loc_t>();
    test_locator_from_iterator<gil::argb32f_loc_t>();

    test_locator_from_iterator<gil::bgr8_loc_t>();
    test_locator_from_iterator<gil::bgr16_loc_t>();
    test_locator_from_iterator<gil::bgr32_loc_t>();
    test_locator_from_iterator<gil::bgr32f_loc_t>();

    test_locator_from_iterator<gil::bgra8_loc_t>();
    test_locator_from_iterator<gil::bgra16_loc_t>();
    test_locator_from_iterator<gil::bgra32_loc_t>();
    test_locator_from_iterator<gil::bgra32f_loc_t>();

    test_locator_from_iterator<gil::rgb8_loc_t>();
    test_locator_from_iterator<gil::rgb8_planar_loc_t>();
    test_locator_from_iterator<gil::rgb16_loc_t>();
    test_locator_from_iterator<gil::rgb16_planar_loc_t>();
    test_locator_from_iterator<gil::rgb32_loc_t>();
    test_locator_from_iterator<gil::rgb32_planar_loc_t>();
    test_locator_from_iterator<gil::rgb32f_loc_t>();
    test_locator_from_iterator<gil::rgb32f_planar_loc_t>();

    test_locator_from_iterator<gil::rgba8_loc_t>();
    test_locator_from_iterator<gil::rgba8_planar_loc_t>();
    test_locator_from_iterator<gil::rgba16_loc_t>();
    test_locator_from_iterator<gil::rgba16_planar_loc_t>();
    test_locator_from_iterator<gil::rgba32_loc_t>();
    test_locator_from_iterator<gil::rgba32_planar_loc_t>();
    test_locator_from_iterator<gil::rgba32f_loc_t>();
    test_locator_from_iterator<gil::rgba32f_planar_loc_t>();

    test_locator_from_iterator<gil::cmyk8_loc_t>();
    test_locator_from_iterator<gil::cmyk8_planar_loc_t>();
    test_locator_from_iterator<gil::cmyk16_loc_t>();
    test_locator_from_iterator<gil::cmyk16_planar_loc_t>();
    test_locator_from_iterator<gil::cmyk32_loc_t>();
    test_locator_from_iterator<gil::cmyk32_planar_loc_t>();
    test_locator_from_iterator<gil::cmyk32f_loc_t>();
    test_locator_from_iterator<gil::cmyk32f_planar_loc_t>();
}
