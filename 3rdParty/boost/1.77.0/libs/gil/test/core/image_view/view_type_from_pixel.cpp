//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>

#include <type_traits>

namespace gil = boost::gil;

struct Interleaved : std::false_type {};
struct Planar : std::true_type {};
struct NotStepX : std::false_type {};
struct StepX : std::true_type {};
struct Immutable : std::false_type {};
struct Mutable : std::true_type {};

template <typename ResultView, typename Pixel, typename IsPlanar, typename IsStepX, typename IsMutable>
void test()
{
    static_assert(std::is_same
    <
        typename gil::view_type_from_pixel
        <
            Pixel,
            IsPlanar::value,
            IsStepX::value,
            IsMutable::value
        >::type,
        ResultView
    >::value, "view_type_from_pixel yields unexpected view");
}

template <typename ResultView, typename Pixel, typename IsPlanar, typename IsStepX, typename IsMutable>
void test_not()
{
    static_assert(!std::is_same
    <
        typename gil::view_type_from_pixel
        <
            Pixel,
            IsPlanar::value,
            IsStepX::value,
            IsMutable::value
        >::type,
        ResultView
    >::value, "view_type_from_pixel yields unexpected view");
}

int main()
{
    test<gil::gray8_view_t, gil::gray8_pixel_t, Interleaved, NotStepX, Mutable>();
    test<gil::gray8c_view_t, gil::gray8c_pixel_t, Interleaved, NotStepX, Immutable>();
    // Immutable view from mutable pixel type is allowed
    test<gil::gray8c_view_t, gil::gray8_pixel_t, Interleaved, NotStepX, Immutable>();
    // Mutable view from immutable pixel type not allowed
    test_not<gil::gray8_view_t, gil::gray8c_pixel_t, Interleaved, NotStepX, Mutable>();

    test<gil::gray8_step_view_t, gil::gray8_pixel_t, Interleaved, StepX, Mutable>();
    test_not<gil::gray8_view_t, gil::gray8c_pixel_t, Interleaved, NotStepX, Mutable>();
    test<gil::gray8c_step_view_t, gil::gray8_pixel_t, Interleaved, StepX, Immutable>();
    test_not<gil::gray8_view_t, gil::gray8_pixel_t, Planar, NotStepX, Mutable>();
    test_not<gil::gray8_view_t, gil::rgb8_pixel_t, Planar, NotStepX, Mutable>();
    test_not<gil::gray8_view_t, gil::gray8_pixel_t, Interleaved, StepX, Mutable>();
    test_not<gil::gray8_view_t, gil::gray8_pixel_t, Interleaved, NotStepX, Immutable>();

    test<gil::abgr8_view_t, gil::abgr8_pixel_t, Interleaved, NotStepX, Mutable>();
    test<gil::abgr8c_view_t, gil::abgr8_pixel_t, Interleaved, NotStepX, Immutable>();
    test<gil::abgr8_step_view_t, gil::abgr8_pixel_t, Interleaved, StepX, Mutable>();
    test<gil::abgr8c_step_view_t, gil::abgr8_pixel_t, Interleaved, StepX, Immutable>();
    test_not<gil::abgr8_view_t, gil::bgra8_pixel_t, Interleaved, NotStepX, Mutable>();
    test_not<gil::abgr8_view_t, gil::rgba8_pixel_t, Interleaved, NotStepX, Mutable>();

    test<gil::argb8_view_t, gil::argb8_pixel_t, Interleaved, NotStepX, Mutable>();
    test<gil::argb8c_view_t, gil::argb8_pixel_t, Interleaved, NotStepX, Immutable>();
    test<gil::argb8_step_view_t, gil::argb8_pixel_t, Interleaved, StepX, Mutable>();
    test<gil::argb8c_step_view_t, gil::argb8_pixel_t, Interleaved, StepX, Immutable>();

    test<gil::bgr8_view_t, gil::bgr8_pixel_t, Interleaved, NotStepX, Mutable>();
    test<gil::bgr8c_view_t, gil::bgr8_pixel_t, Interleaved, NotStepX, Immutable>();
    test<gil::bgr8_step_view_t, gil::bgr8_pixel_t, Interleaved, StepX, Mutable>();
    test<gil::bgr8c_step_view_t, gil::bgr8_pixel_t, Interleaved, StepX, Immutable>();
    test_not<gil::bgr8_view_t, gil::rgb8_pixel_t, Interleaved, NotStepX, Mutable>();

    test<gil::bgra8_view_t, gil::bgra8_pixel_t, Interleaved, NotStepX, Mutable>();
    test<gil::bgra8c_view_t, gil::bgra8_pixel_t, Interleaved, NotStepX, Immutable>();
    test<gil::bgra8_step_view_t, gil::bgra8_pixel_t, Interleaved, StepX, Mutable>();
    test<gil::bgra8c_step_view_t, gil::bgra8_pixel_t, Interleaved, StepX, Immutable>();
    test_not<gil::bgra8_view_t, gil::abgr8_pixel_t, Interleaved, NotStepX, Mutable>();
    test_not<gil::bgra8_view_t, gil::rgba8_pixel_t, Interleaved, NotStepX, Mutable>();

    test<gil::rgb8_view_t, gil::rgb8_pixel_t, Interleaved, NotStepX, Mutable>();
    test<gil::rgb8c_view_t, gil::rgb8_pixel_t, Interleaved, NotStepX, Immutable>();
    test<gil::rgb8_step_view_t, gil::rgb8_pixel_t, Interleaved, StepX, Mutable>();
    test<gil::rgb8c_step_view_t, gil::rgb8_pixel_t, Interleaved, StepX, Immutable>();
    test_not<gil::rgb8_view_t, gil::rgb8c_pixel_t, Interleaved, NotStepX, Mutable>();
    test_not<gil::rgb8_view_t, gil::rgb8_pixel_t, Planar, NotStepX, Mutable>();
    test_not<gil::rgb8_view_t, gil::abgr8_pixel_t, Interleaved, NotStepX, Mutable>();
    test_not<gil::rgb8_view_t, gil::bgra8_pixel_t, Interleaved, NotStepX, Mutable>();

    test<gil::rgb8_planar_view_t, gil::rgb8_pixel_t, Planar, NotStepX, Mutable>();
    test<gil::rgb8c_planar_view_t, gil::rgb8_pixel_t, Planar, NotStepX, Immutable>();
    test<gil::rgb8_planar_step_view_t, gil::rgb8_pixel_t, Planar, StepX, Mutable>();
    test<gil::rgb8c_planar_step_view_t, gil::rgb8_pixel_t, Planar, StepX, Immutable>();

    test<gil::cmyk8_view_t, gil::cmyk8_pixel_t, Interleaved, NotStepX, Mutable>();
    test<gil::cmyk8c_view_t, gil::cmyk8_pixel_t, Interleaved, NotStepX, Immutable>();
    test<gil::cmyk8_step_view_t, gil::cmyk8_pixel_t, Interleaved, StepX, Mutable>();
    test<gil::cmyk8c_step_view_t, gil::cmyk8_pixel_t, Interleaved, StepX, Immutable>();
    test_not<gil::cmyk8_view_t, gil::rgba8_pixel_t, Interleaved, NotStepX, Mutable>();

    test<gil::cmyk8_planar_view_t, gil::cmyk8_pixel_t, Planar, NotStepX, Mutable>();
    test<gil::cmyk8c_planar_view_t, gil::cmyk8_pixel_t, Planar, NotStepX, Immutable>();
    test<gil::cmyk8_planar_step_view_t, gil::cmyk8_pixel_t, Planar, StepX, Mutable>();
    test<gil::cmyk8c_planar_step_view_t, gil::cmyk8_pixel_t, Planar, StepX, Immutable>();
}
