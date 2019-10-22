//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_USE_CONCEPT_CHECK
#error Compile with BOOST_GIL_USE_CONCEPT_CHECK defined
#endif
#include <boost/gil/concepts.hpp>
#include <boost/gil/color_base.hpp> // kth_element_type
#include <boost/gil/pixel.hpp> // kth_element_type
#include <boost/gil/pixel_iterator.hpp>
#include <boost/gil/pixel_iterator_adaptor.hpp>
#include <boost/gil/planar_pixel_iterator.hpp> // kth_element_type
#include <boost/gil/planar_pixel_reference.hpp> // kth_element_type
#include <boost/gil/typedefs.hpp>

#include <boost/concept_check.hpp>

namespace gil = boost::gil;
using boost::function_requires;

template <typename Iterator>
void test_immutable()
{
    function_requires<gil::PixelIteratorConcept<Iterator>>();
    function_requires<gil::HasDynamicXStepTypeConcept<Iterator>>();
    // NOTE: Pixel iterator does not model Y-step
    //function_requires<gil::HasDynamicYStepTypeConcept<Iterator>>();
}

template <typename Iterator>
void test_mutable()
{
    function_requires<gil::MutablePixelIteratorConcept<Iterator>>();
    function_requires<gil::HasDynamicXStepTypeConcept<Iterator>>();
    // NOTE: Pixel iterator does not model Y-step
    //function_requires<gil::HasDynamicYStepTypeConcept<Iterator>>();
}

int main()
{
    test_mutable<gil::gray8_ptr_t>();
    test_mutable<gil::gray8_step_ptr_t>();
    test_mutable<gil::gray16_ptr_t>();
    test_mutable<gil::gray16_step_ptr_t>();
    test_mutable<gil::gray32_ptr_t>();
    test_mutable<gil::gray32_step_ptr_t>();
    test_mutable<gil::gray32f_ptr_t>();
    test_mutable<gil::gray32f_step_ptr_t>();
    test_immutable<gil::gray8c_ptr_t>();
    test_immutable<gil::gray8c_step_ptr_t>();
    test_immutable<gil::gray16c_ptr_t>();
    test_immutable<gil::gray16c_step_ptr_t>();
    test_immutable<gil::gray32c_ptr_t>();
    test_immutable<gil::gray32c_step_ptr_t>();
    test_immutable<gil::gray32fc_ptr_t>();
    test_immutable<gil::gray32fc_step_ptr_t>();

    test_mutable<gil::abgr8_ptr_t>();
    test_mutable<gil::abgr8_step_ptr_t>();
    test_mutable<gil::abgr16_ptr_t>();
    test_mutable<gil::abgr16_step_ptr_t>();
    test_mutable<gil::abgr32_ptr_t>();
    test_mutable<gil::abgr32_step_ptr_t>();
    test_mutable<gil::abgr32f_ptr_t>();
    test_mutable<gil::abgr32f_step_ptr_t>();
    test_immutable<gil::abgr8c_ptr_t>();
    test_immutable<gil::abgr8c_step_ptr_t>();
    test_immutable<gil::abgr16c_ptr_t>();
    test_immutable<gil::abgr16c_step_ptr_t>();
    test_immutable<gil::abgr32c_ptr_t>();
    test_immutable<gil::abgr32c_step_ptr_t>();
    test_immutable<gil::abgr32fc_ptr_t>();
    test_immutable<gil::abgr32fc_step_ptr_t>();

    test_mutable<gil::argb8_ptr_t>();
    test_mutable<gil::argb8_step_ptr_t>();
    test_mutable<gil::argb16_ptr_t>();
    test_mutable<gil::argb16_step_ptr_t>();
    test_mutable<gil::argb32_ptr_t>();
    test_mutable<gil::argb32_step_ptr_t>();
    test_mutable<gil::argb32f_ptr_t>();
    test_mutable<gil::argb32f_step_ptr_t>();

    test_mutable<gil::bgr8_ptr_t>();
    test_mutable<gil::bgr8_step_ptr_t>();
    test_mutable<gil::bgr16_ptr_t>();
    test_mutable<gil::bgr16_step_ptr_t>();
    test_mutable<gil::bgr32_ptr_t>();
    test_mutable<gil::bgr32_step_ptr_t>();
    test_mutable<gil::bgr32f_ptr_t>();
    test_mutable<gil::bgr32f_step_ptr_t>();

    test_mutable<gil::bgra8_ptr_t>();
    test_mutable<gil::bgra8_step_ptr_t>();
    test_mutable<gil::bgra16_ptr_t>();
    test_mutable<gil::bgra16_step_ptr_t>();
    test_mutable<gil::bgra32_ptr_t>();
    test_mutable<gil::bgra32_step_ptr_t>();
    test_mutable<gil::bgra32f_ptr_t>();
    test_mutable<gil::bgra32f_step_ptr_t>();

    test_mutable<gil::rgb8_ptr_t>();
    test_mutable<gil::rgb8_step_ptr_t>();
    test_mutable<gil::rgb8_planar_ptr_t>();
    test_mutable<gil::rgb8_planar_step_ptr_t>();
    test_mutable<gil::rgb16_ptr_t>();
    test_mutable<gil::rgb16_step_ptr_t>();
    test_mutable<gil::rgb16_planar_ptr_t>();
    test_mutable<gil::rgb16_planar_step_ptr_t>();
    test_mutable<gil::rgb32_ptr_t>();
    test_mutable<gil::rgb32_step_ptr_t>();
    test_mutable<gil::rgb32_planar_ptr_t>();
    test_mutable<gil::rgb32_planar_step_ptr_t>();
    test_mutable<gil::rgb32f_ptr_t>();
    test_mutable<gil::rgb32f_step_ptr_t>();
    test_mutable<gil::rgb32f_planar_ptr_t>();
    test_mutable<gil::rgb32f_planar_step_ptr_t>();
    test_immutable<gil::rgb8c_ptr_t>();
    test_immutable<gil::rgb8c_step_ptr_t>();
    test_immutable<gil::rgb16c_ptr_t>();
    test_immutable<gil::rgb16c_step_ptr_t>();
    test_immutable<gil::rgb32c_ptr_t>();
    test_immutable<gil::rgb32c_step_ptr_t>();
    test_immutable<gil::rgb32fc_ptr_t>();
    test_immutable<gil::rgb32fc_step_ptr_t>();

    test_mutable<gil::rgba8_ptr_t>();
    test_mutable<gil::rgba8_step_ptr_t>();
    test_mutable<gil::rgba8_planar_ptr_t>();
    test_mutable<gil::rgba8_planar_step_ptr_t>();
    test_mutable<gil::rgba16_ptr_t>();
    test_mutable<gil::rgba16_step_ptr_t>();
    test_mutable<gil::rgba16_planar_ptr_t>();
    test_mutable<gil::rgba16_planar_step_ptr_t>();
    test_mutable<gil::rgba32_ptr_t>();
    test_mutable<gil::rgba32_step_ptr_t>();
    test_mutable<gil::rgba32_planar_ptr_t>();
    test_mutable<gil::rgba32_planar_step_ptr_t>();
    test_mutable<gil::rgba32f_ptr_t>();
    test_mutable<gil::rgba32f_step_ptr_t>();
    test_mutable<gil::rgba32f_planar_ptr_t>();
    test_mutable<gil::rgba32f_planar_step_ptr_t>();

    test_mutable<gil::cmyk8_ptr_t>();
    test_mutable<gil::cmyk8_step_ptr_t>();
    test_mutable<gil::cmyk8_planar_ptr_t>();
    test_mutable<gil::cmyk8_planar_step_ptr_t>();
    test_mutable<gil::cmyk16_ptr_t>();
    test_mutable<gil::cmyk16_step_ptr_t>();
    test_mutable<gil::cmyk16_planar_ptr_t>();
    test_mutable<gil::cmyk16_planar_step_ptr_t>();
    test_mutable<gil::cmyk32_ptr_t>();
    test_mutable<gil::cmyk32_step_ptr_t>();
    test_mutable<gil::cmyk32_planar_ptr_t>();
    test_mutable<gil::cmyk32_planar_step_ptr_t>();
    test_mutable<gil::cmyk32f_ptr_t>();
    test_mutable<gil::cmyk32f_step_ptr_t>();
    test_mutable<gil::cmyk32f_planar_ptr_t>();
    test_mutable<gil::cmyk32f_planar_step_ptr_t>();
    test_immutable<gil::cmyk8c_ptr_t>();
    test_immutable<gil::cmyk8c_step_ptr_t>();
    test_immutable<gil::cmyk8c_planar_ptr_t>();
    test_immutable<gil::cmyk8c_planar_step_ptr_t>();
}
