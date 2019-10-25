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
#include <boost/gil/pixel_iterator.hpp>
#include <boost/gil/pixel_iterator_adaptor.hpp>
#include <boost/gil/planar_pixel_iterator.hpp> // kth_element_type
#include <boost/gil/planar_pixel_reference.hpp> // kth_element_type
#include <boost/gil/step_iterator.hpp>
#include <boost/gil/typedefs.hpp>

#include <type_traits>

namespace gil = boost::gil;

// NOTE: Iterator does not model HasDynamicYStepTypeConcept
// TODO: Add compile-fail tests to verify that.

template <typename Iterator>
void test_non_step()
{
    static_assert(std::is_same
        <
            gil::memory_based_step_iterator<Iterator>,
            typename gil::dynamic_x_step_type<Iterator>::type
        >::value, "iterator does not model HasDynamicXStepTypeConcept");

    static_assert(!std::is_same
        <
            Iterator,
            typename gil::dynamic_x_step_type<Iterator>::type
        >::value, "dynamic_x_step_type yields X step iterator, not X iterator");
}

template <typename Iterator>
void test_step()
{
    static_assert(std::is_same
        <
            Iterator,
            typename gil::dynamic_x_step_type<Iterator>::type
        >::value, "iterator does not model HasDynamicXStepTypeConcept");

    static_assert(!std::is_same
        <
            typename Iterator::x_iterator,
            typename gil::dynamic_x_step_type<gil::gray8_step_ptr_t>::type
        >::value, "dynamic_x_step_type yields X step iterator, not X iterator");
}

int main()
{
    test_non_step<gil::gray8_ptr_t>();
    test_non_step<gil::gray8c_ptr_t>();
    test_non_step<gil::gray16_ptr_t>();
    test_non_step<gil::gray16c_ptr_t>();
    test_non_step<gil::gray32_ptr_t>();
    test_non_step<gil::gray32c_ptr_t>();
    test_non_step<gil::gray32f_ptr_t>();
    test_step<gil::gray8_step_ptr_t>();
    test_step<gil::gray8c_step_ptr_t>();
    test_step<gil::gray16_step_ptr_t>();
    test_step<gil::gray16c_step_ptr_t>();
    test_step<gil::gray32_step_ptr_t>();
    test_step<gil::gray32c_step_ptr_t>();
    test_step<gil::gray32f_step_ptr_t>();

    test_non_step<gil::abgr8_ptr_t>();
    test_non_step<gil::abgr16_ptr_t>();
    test_non_step<gil::abgr32_ptr_t>();
    test_non_step<gil::abgr32f_ptr_t>();
    test_step<gil::abgr8_step_ptr_t>();
    test_step<gil::abgr16_step_ptr_t>();
    test_step<gil::abgr32_step_ptr_t>();
    test_step<gil::abgr32f_step_ptr_t>();

    test_non_step<gil::argb8_ptr_t>();
    test_non_step<gil::argb16_ptr_t>();
    test_non_step<gil::argb32_ptr_t>();
    test_non_step<gil::argb32f_ptr_t>();
    test_step<gil::argb8_step_ptr_t>();
    test_step<gil::argb16_step_ptr_t>();
    test_step<gil::argb32_step_ptr_t>();
    test_step<gil::argb32f_step_ptr_t>();

    test_non_step<gil::bgr8_ptr_t>();
    test_non_step<gil::bgr16_ptr_t>();
    test_non_step<gil::bgr32_ptr_t>();
    test_non_step<gil::bgr32f_ptr_t>();
    test_step<gil::bgr8_step_ptr_t>();
    test_step<gil::bgr16_step_ptr_t>();
    test_step<gil::bgr32_step_ptr_t>();
    test_step<gil::bgr32f_step_ptr_t>();

    test_non_step<gil::bgra8_ptr_t>();
    test_non_step<gil::bgra16_ptr_t>();
    test_non_step<gil::bgra32_ptr_t>();
    test_non_step<gil::bgra32f_ptr_t>();
    test_step<gil::bgra8_step_ptr_t>();
    test_step<gil::bgra16_step_ptr_t>();
    test_step<gil::bgra32_step_ptr_t>();
    test_step<gil::bgra32f_step_ptr_t>();

    test_non_step<gil::rgb8_ptr_t>();
    test_non_step<gil::rgb8_planar_ptr_t>();
    test_non_step<gil::rgb16_ptr_t>();
    test_non_step<gil::rgb16_planar_ptr_t>();
    test_non_step<gil::rgb32_ptr_t>();
    test_non_step<gil::rgb32_planar_ptr_t>();
    test_non_step<gil::rgb32f_ptr_t>();
    test_non_step<gil::rgb32f_planar_ptr_t>();
    test_step<gil::rgb8_step_ptr_t>();
    test_step<gil::rgb8_planar_step_ptr_t>();
    test_step<gil::rgb16_step_ptr_t>();
    test_step<gil::rgb16_planar_step_ptr_t>();
    test_step<gil::rgb32_step_ptr_t>();
    test_step<gil::rgb32_planar_step_ptr_t>();
    test_step<gil::rgb32f_step_ptr_t>();
    test_step<gil::rgb32f_planar_step_ptr_t>();

    test_non_step<gil::rgba8_ptr_t>();
    test_non_step<gil::rgba8_planar_ptr_t>();
    test_non_step<gil::rgba16_ptr_t>();
    test_non_step<gil::rgba16_planar_ptr_t>();
    test_non_step<gil::rgba32_ptr_t>();
    test_non_step<gil::rgba32_planar_ptr_t>();
    test_non_step<gil::rgba32f_ptr_t>();
    test_non_step<gil::rgba32f_planar_ptr_t>();
    test_step<gil::rgba8_step_ptr_t>();
    test_step<gil::rgba8_planar_step_ptr_t>();
    test_step<gil::rgba16_step_ptr_t>();
    test_step<gil::rgba16_planar_step_ptr_t>();
    test_step<gil::rgba32_step_ptr_t>();
    test_step<gil::rgba32_planar_step_ptr_t>();
    test_step<gil::rgba32f_step_ptr_t>();
    test_step<gil::rgba32f_planar_step_ptr_t>();

    test_non_step<gil::cmyk8_ptr_t>();
    test_non_step<gil::cmyk8_planar_ptr_t>();
    test_non_step<gil::cmyk16_ptr_t>();
    test_non_step<gil::cmyk16_planar_ptr_t>();
    test_non_step<gil::cmyk32_ptr_t>();
    test_non_step<gil::cmyk32_planar_ptr_t>();
    test_non_step<gil::cmyk32f_ptr_t>();
    test_non_step<gil::cmyk32f_planar_ptr_t>();
    test_step<gil::cmyk8_step_ptr_t>();
    test_step<gil::cmyk8_planar_step_ptr_t>();
    test_step<gil::cmyk16_step_ptr_t>();
    test_step<gil::cmyk16_planar_step_ptr_t>();
    test_step<gil::cmyk32_step_ptr_t>();
    test_step<gil::cmyk32_planar_step_ptr_t>();
    test_step<gil::cmyk32f_step_ptr_t>();
    test_step<gil::cmyk32f_planar_step_ptr_t>();
}
