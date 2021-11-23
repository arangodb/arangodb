//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// FIXME: Avoid Clang's flooding of non-disableable warnings like:
// "T does not declare any constructor to initialize its non-modifiable members"
// when compiling with concepts check enabled.
// See https://bugs.llvm.org/show_bug.cgi?id=41759
#if !defined(BOOST_GIL_USE_CONCEPT_CHECK) && !defined(__clang__)
#error Compile with BOOST_GIL_USE_CONCEPT_CHECK defined
#endif
#include <boost/gil/concepts.hpp>
#include <boost/gil/locator.hpp>
#include <boost/gil/planar_pixel_reference.hpp>
#include <boost/gil/typedefs.hpp>
#include <boost/gil/virtual_locator.hpp>

#include <boost/concept_check.hpp>

namespace gil = boost::gil;
using boost::function_requires;

template <typename Locator>
void test_immutable()
{
    function_requires<gil::PixelLocatorConcept<Locator>>();
    function_requires<gil::HasDynamicXStepTypeConcept<Locator>>();
    function_requires<gil::HasDynamicYStepTypeConcept<Locator>>();
}

template <typename Locator>
void test_mutable()
{
    function_requires<gil::MutablePixelLocatorConcept<Locator>>();
    function_requires<gil::HasDynamicXStepTypeConcept<Locator>>();
    function_requires<gil::HasDynamicYStepTypeConcept<Locator>>();
}

template <typename Pixel>
struct archetype_pixel_dereference
{
    using point_t = gil::point_t;
    using const_t = archetype_pixel_dereference;
    using value_type = Pixel;
    using reference = value_type;
    using const_reference = value_type;
    using argument_type = gil::point_t;
    using result_type = reference;
    static constexpr bool is_mutable = false;
    result_type operator()(argument_type const&) const { return result_type{}; }
};

template <typename Pixel>
using virtual_locator = gil::virtual_2d_locator<archetype_pixel_dereference<Pixel>, false>;

int main()
{
    test_mutable<gil::gray8_loc_t>();
    test_mutable<gil::gray8_step_loc_t>();
    test_mutable<gil::gray16_loc_t>();
    test_mutable<gil::gray16_step_loc_t>();
    test_mutable<gil::gray32_loc_t>();
    test_mutable<gil::gray32_step_loc_t>();
    test_mutable<gil::gray32f_loc_t>();
    test_mutable<gil::gray32f_step_loc_t>();
    test_immutable<gil::gray8c_loc_t>();
    test_immutable<gil::gray8c_step_loc_t>();
    test_immutable<gil::gray16c_loc_t>();
    test_immutable<gil::gray16c_step_loc_t>();
    test_immutable<gil::gray32c_loc_t>();
    test_immutable<gil::gray32c_step_loc_t>();
    test_immutable<gil::gray32fc_loc_t>();
    test_immutable<gil::gray32fc_step_loc_t>();

    test_mutable<gil::memory_based_2d_locator<gil::gray8_step_ptr_t>>();
    test_immutable<gil::memory_based_2d_locator<gil::gray8c_step_ptr_t>>();
    test_mutable<virtual_locator<gil::gray8_pixel_t>>();
    test_immutable<virtual_locator<gil::gray8c_pixel_t>>();

    test_mutable<gil::abgr8_loc_t>();
    test_mutable<gil::abgr8_step_loc_t>();
    test_mutable<gil::memory_based_2d_locator<gil::abgr8_step_ptr_t>>();
    test_mutable<gil::abgr16_loc_t>();
    test_mutable<gil::abgr16_step_loc_t>();
    test_mutable<gil::abgr32_loc_t>();
    test_mutable<gil::abgr32_step_loc_t>();
    test_mutable<gil::abgr32f_loc_t>();
    test_mutable<gil::abgr32f_step_loc_t>();
    test_immutable<gil::abgr8c_loc_t>();
    test_immutable<gil::abgr8c_step_loc_t>();
    test_immutable<gil::memory_based_2d_locator<gil::abgr8c_step_ptr_t>>();
    test_immutable<gil::abgr16c_loc_t>();
    test_immutable<gil::abgr16c_step_loc_t>();
    test_immutable<gil::abgr32c_loc_t>();
    test_immutable<gil::abgr32c_step_loc_t>();
    test_immutable<gil::abgr32fc_loc_t>();
    test_immutable<gil::abgr32fc_step_loc_t>();

    test_mutable<gil::argb8_loc_t>();
    test_mutable<gil::argb8_step_loc_t>();
    test_mutable<gil::memory_based_2d_locator<gil::argb8_step_ptr_t>>();
    test_mutable<gil::argb16_loc_t>();
    test_mutable<gil::argb16_step_loc_t>();
    test_mutable<gil::argb32_loc_t>();
    test_mutable<gil::argb32_step_loc_t>();
    test_mutable<gil::argb32f_loc_t>();
    test_mutable<gil::argb32f_step_loc_t>();

    test_mutable<gil::bgr8_loc_t>();
    test_mutable<gil::bgr8_step_loc_t>();
    test_mutable<gil::memory_based_2d_locator<gil::bgr8_step_ptr_t>>();
    test_mutable<gil::bgr16_loc_t>();
    test_mutable<gil::bgr16_step_loc_t>();
    test_mutable<gil::bgr32_loc_t>();
    test_mutable<gil::bgr32_step_loc_t>();
    test_mutable<gil::bgr32f_loc_t>();
    test_mutable<gil::bgr32f_step_loc_t>();

    test_mutable<gil::bgra8_loc_t>();
    test_mutable<gil::bgra8_step_loc_t>();
    test_mutable<gil::memory_based_2d_locator<gil::bgra8_step_ptr_t>>();
    test_mutable<gil::bgra16_loc_t>();
    test_mutable<gil::bgra16_step_loc_t>();
    test_mutable<gil::bgra32_loc_t>();
    test_mutable<gil::bgra32_step_loc_t>();
    test_mutable<gil::bgra32f_loc_t>();
    test_mutable<gil::bgra32f_step_loc_t>();

    test_mutable<gil::rgb8_loc_t>();
    test_mutable<gil::rgb8_step_loc_t>();
    test_mutable<gil::rgb8_planar_loc_t>();
    test_mutable<gil::rgb8_planar_step_loc_t>();
    test_mutable<gil::rgb16_loc_t>();
    test_mutable<gil::rgb16_step_loc_t>();
    test_mutable<gil::rgb16_planar_loc_t>();
    test_mutable<gil::rgb16_planar_step_loc_t>();
    test_mutable<gil::rgb32_loc_t>();
    test_mutable<gil::rgb32_step_loc_t>();
    test_mutable<gil::rgb32_planar_loc_t>();
    test_mutable<gil::rgb32_planar_step_loc_t>();
    test_mutable<gil::rgb32f_loc_t>();
    test_mutable<gil::rgb32f_step_loc_t>();
    test_mutable<gil::rgb32f_planar_loc_t>();
    test_mutable<gil::rgb32f_planar_step_loc_t>();
    test_immutable<gil::rgb8c_loc_t>();
    test_immutable<gil::rgb8c_step_loc_t>();
    test_immutable<gil::rgb16c_loc_t>();
    test_immutable<gil::rgb16c_step_loc_t>();
    test_immutable<gil::rgb32c_loc_t>();
    test_immutable<gil::rgb32c_step_loc_t>();
    test_immutable<gil::rgb32fc_loc_t>();
    test_immutable<gil::rgb32fc_step_loc_t>();

    test_mutable<gil::memory_based_2d_locator<gil::rgb8_step_ptr_t>>();
    test_immutable<gil::memory_based_2d_locator<gil::rgb8c_step_ptr_t>>();
    test_mutable<virtual_locator<gil::rgb8_pixel_t>>();
    test_immutable<virtual_locator<gil::rgb8c_pixel_t>>();

    test_mutable<gil::rgba8_loc_t>();
    test_mutable<gil::rgba8_step_loc_t>();
    test_mutable<gil::rgba8_planar_loc_t>();
    test_mutable<gil::rgba8_planar_step_loc_t>();
    test_mutable<gil::memory_based_2d_locator<gil::rgba8_step_ptr_t>>();
    test_mutable<gil::rgba16_loc_t>();
    test_mutable<gil::rgba16_step_loc_t>();
    test_mutable<gil::rgba16_planar_loc_t>();
    test_mutable<gil::rgba16_planar_step_loc_t>();
    test_mutable<gil::rgba32_loc_t>();
    test_mutable<gil::rgba32_step_loc_t>();
    test_mutable<gil::rgba32_planar_loc_t>();
    test_mutable<gil::rgba32_planar_step_loc_t>();
    test_mutable<gil::rgba32f_loc_t>();
    test_mutable<gil::rgba32f_step_loc_t>();
    test_mutable<gil::rgba32f_planar_loc_t>();
    test_mutable<gil::rgba32f_planar_step_loc_t>();

    test_mutable<gil::cmyk8_loc_t>();
    test_mutable<gil::cmyk8_step_loc_t>();
    test_mutable<gil::cmyk8_planar_loc_t>();
    test_mutable<gil::cmyk8_planar_step_loc_t>();
    test_mutable<gil::memory_based_2d_locator<gil::cmyk8_step_ptr_t>>();
    test_mutable<gil::cmyk16_loc_t>();
    test_mutable<gil::cmyk16_step_loc_t>();
    test_mutable<gil::cmyk16_planar_loc_t>();
    test_mutable<gil::cmyk16_planar_step_loc_t>();
    test_mutable<gil::cmyk32_loc_t>();
    test_mutable<gil::cmyk32_step_loc_t>();
    test_mutable<gil::cmyk32_planar_loc_t>();
    test_mutable<gil::cmyk32_planar_step_loc_t>();
    test_mutable<gil::cmyk32f_loc_t>();
    test_mutable<gil::cmyk32f_step_loc_t>();
    test_mutable<gil::cmyk32f_planar_loc_t>();
    test_mutable<gil::cmyk32f_planar_step_loc_t>();
    test_immutable<gil::cmyk8c_loc_t>();
    test_immutable<gil::cmyk8c_step_loc_t>();
    test_immutable<gil::cmyk8c_planar_loc_t>();
    test_immutable<gil::cmyk8c_planar_step_loc_t>();
    test_immutable<gil::memory_based_2d_locator<gil::cmyk8c_step_ptr_t>>();
}
