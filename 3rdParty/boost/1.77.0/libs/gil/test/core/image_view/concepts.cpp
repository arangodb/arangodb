//
// Copyright 2018 Mateusz Loskot <mateusz at loskot dot net>
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
// FIXME: There are missing headers internally, leading to incomplete types
#if 0
#include <boost/gil/image_view.hpp>
#include <boost/gil/planar_pixel_reference.hpp>
#include <boost/gil/typedefs.hpp>
#else
#include <boost/gil.hpp>
#endif

namespace gil = boost::gil;

template <typename View>
void test_view()
{
    boost::function_requires<gil::ImageViewConcept<View>>();
    boost::function_requires<gil::CollectionImageViewConcept<View>>();
    boost::function_requires<gil::ForwardCollectionImageViewConcept<View>>();
    boost::function_requires<gil::ReversibleCollectionImageViewConcept<View>>();
}

template <typename View>
void test_view_planar()
{
    boost::function_requires<gil::ImageViewConcept<View>>();
    boost::function_requires<gil::CollectionImageViewConcept<View>>();
}

int main()
{
    test_view<gil::gray8_view_t>();
    test_view<gil::gray8c_view_t>();

    test_view<gil::abgr8_view_t>();
    test_view<gil::abgr8_step_view_t>();
    test_view<gil::abgr8c_view_t>();
    test_view<gil::abgr8c_step_view_t>();

    test_view<gil::argb8_view_t>();
    test_view<gil::argb8_step_view_t>();
    test_view<gil::argb8c_view_t>();
    test_view<gil::argb8c_step_view_t>();

    test_view<gil::bgr8_view_t>();
    test_view<gil::bgr8_step_view_t>();
    test_view<gil::bgr8c_view_t>();
    test_view<gil::bgr8c_step_view_t>();

    test_view<gil::bgra8_view_t>();
    test_view<gil::bgra8_step_view_t>();
    test_view<gil::bgra8c_view_t>();
    test_view<gil::bgra8c_step_view_t>();

    test_view<gil::rgb8_view_t>();
    test_view<gil::rgb8_step_view_t>();
    test_view<gil::rgb8c_view_t>();
    test_view<gil::rgb8c_step_view_t>();
    test_view_planar<gil::rgb8_planar_view_t>();
    test_view_planar<gil::rgb8_planar_step_view_t>();
    test_view_planar<gil::rgb8c_planar_view_t>();
    test_view_planar<gil::rgb8c_planar_step_view_t>();

    test_view<gil::rgba8_view_t>();
    test_view<gil::rgba8_step_view_t>();
    test_view<gil::rgba8c_view_t>();
    test_view<gil::rgba8c_step_view_t>();
    test_view_planar<gil::rgba8_planar_view_t>();
    test_view_planar<gil::rgba8_planar_step_view_t>();
    test_view_planar<gil::rgba8c_planar_view_t>();
    test_view_planar<gil::rgba8c_planar_step_view_t>();

    test_view<gil::cmyk8_view_t>();
    test_view<gil::cmyk8_step_view_t>();
    test_view<gil::cmyk8c_view_t>();
    test_view<gil::cmyk8c_step_view_t>();

    test_view_planar<gil::cmyk8_planar_view_t>();
    test_view_planar<gil::cmyk8_planar_step_view_t>();
    test_view_planar<gil::cmyk8c_planar_view_t>();
    test_view_planar<gil::cmyk8c_planar_step_view_t>();

    return 0;
}
