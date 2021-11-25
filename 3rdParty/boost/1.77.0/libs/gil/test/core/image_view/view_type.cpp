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

template <typename ResultView, typename Layout, typename IsPlanar, typename IsStepX, typename IsMutable>
void test()
{
    static_assert(std::is_same
    <
        typename gil::view_type
        <
            std::uint8_t,
            Layout,
            IsPlanar::value,
            IsStepX::value,
            IsMutable::value
        >::type,
        ResultView
    >::value, "view_type yields unexpected view");
}

template <typename ResultView, typename Layout, typename IsPlanar, typename IsStepX, typename IsMutable>
void test_not()
{
    static_assert(!std::is_same
    <
        typename gil::view_type
        <
            std::uint8_t,
            Layout,
            IsPlanar::value,
            IsStepX::value,
            IsMutable::value
        >::type,
        ResultView
    >::value, "view_type yields unexpected view");
}

int main()
{
    test<gil::gray8_view_t, gil::gray_layout_t, Interleaved, NotStepX, Mutable>();
    test<gil::gray8c_view_t, gil::gray_layout_t, Interleaved, NotStepX, Immutable>();
    test<gil::gray8_step_view_t, gil::gray_layout_t, Interleaved, StepX, Mutable>();
    test<gil::gray8c_step_view_t, gil::gray_layout_t, Interleaved, StepX, Immutable>();
    test_not<gil::gray8_view_t, gil::gray_layout_t, Planar, NotStepX, Mutable>();
    test_not<gil::gray8_view_t, gil::rgb_layout_t, Planar, NotStepX, Mutable>();
    test_not<gil::gray8_view_t, gil::gray_layout_t, Interleaved, StepX, Mutable>();
    test_not<gil::gray8_view_t, gil::gray_layout_t, Interleaved, NotStepX, Immutable>();

    test<gil::abgr8_view_t, gil::abgr_layout_t, Interleaved, NotStepX, Mutable>();
    test<gil::abgr8c_view_t, gil::abgr_layout_t, Interleaved, NotStepX, Immutable>();
    test<gil::abgr8_step_view_t, gil::abgr_layout_t, Interleaved, StepX, Mutable>();
    test<gil::abgr8c_step_view_t, gil::abgr_layout_t, Interleaved, StepX, Immutable>();
    test_not<gil::abgr8_view_t, gil::bgra_layout_t, Interleaved, NotStepX, Mutable>();
    test_not<gil::abgr8_view_t, gil::rgba_layout_t, Interleaved, NotStepX, Mutable>();

    test<gil::argb8_view_t, gil::argb_layout_t, Interleaved, NotStepX, Mutable>();
    test<gil::argb8c_view_t, gil::argb_layout_t, Interleaved, NotStepX, Immutable>();
    test<gil::argb8_step_view_t, gil::argb_layout_t, Interleaved, StepX, Mutable>();
    test<gil::argb8c_step_view_t, gil::argb_layout_t, Interleaved, StepX, Immutable>();

    test<gil::bgr8_view_t, gil::bgr_layout_t, Interleaved, NotStepX, Mutable>();
    test<gil::bgr8c_view_t, gil::bgr_layout_t, Interleaved, NotStepX, Immutable>();
    test<gil::bgr8_step_view_t, gil::bgr_layout_t, Interleaved, StepX, Mutable>();
    test<gil::bgr8c_step_view_t, gil::bgr_layout_t, Interleaved, StepX, Immutable>();
    test_not<gil::bgr8_view_t, gil::rgb_layout_t, Interleaved, NotStepX, Mutable>();

    test<gil::bgra8_view_t, gil::bgra_layout_t, Interleaved, NotStepX, Mutable>();
    test<gil::bgra8c_view_t, gil::bgra_layout_t, Interleaved, NotStepX, Immutable>();
    test<gil::bgra8_step_view_t, gil::bgra_layout_t, Interleaved, StepX, Mutable>();
    test<gil::bgra8c_step_view_t, gil::bgra_layout_t, Interleaved, StepX, Immutable>();
    test_not<gil::bgra8_view_t, gil::abgr_layout_t, Interleaved, NotStepX, Mutable>();
    test_not<gil::bgra8_view_t, gil::rgba_layout_t, Interleaved, NotStepX, Mutable>();

    test<gil::rgb8_view_t, gil::rgb_layout_t, Interleaved, NotStepX, Mutable>();
    test<gil::rgb8c_view_t, gil::rgb_layout_t, Interleaved, NotStepX, Immutable>();
    test<gil::rgb8_step_view_t, gil::rgb_layout_t, Interleaved, StepX, Mutable>();
    test<gil::rgb8c_step_view_t, gil::rgb_layout_t, Interleaved, StepX, Immutable>();
    test_not<gil::rgb8_view_t, gil::rgb_layout_t, Planar, NotStepX, Mutable>();
    test_not<gil::rgb8_view_t, gil::abgr_layout_t, Interleaved, NotStepX, Mutable>();
    test_not<gil::rgb8_view_t, gil::bgra_layout_t, Interleaved, NotStepX, Mutable>();

    test<gil::rgb8_planar_view_t, gil::rgb_layout_t, Planar, NotStepX, Mutable>();
    test<gil::rgb8c_planar_view_t, gil::rgb_layout_t, Planar, NotStepX, Immutable>();
    test<gil::rgb8_planar_step_view_t, gil::rgb_layout_t, Planar, StepX, Mutable>();
    test<gil::rgb8c_planar_step_view_t, gil::rgb_layout_t, Planar, StepX, Immutable>();

    test<gil::cmyk8_view_t, gil::cmyk_layout_t, Interleaved, NotStepX, Mutable>();
    test<gil::cmyk8c_view_t, gil::cmyk_layout_t, Interleaved, NotStepX, Immutable>();
    test<gil::cmyk8_step_view_t, gil::cmyk_layout_t, Interleaved, StepX, Mutable>();
    test<gil::cmyk8c_step_view_t, gil::cmyk_layout_t, Interleaved, StepX, Immutable>();
    test_not<gil::cmyk8_view_t, gil::rgba_layout_t, Interleaved, NotStepX, Mutable>();

    test<gil::cmyk8_planar_view_t, gil::cmyk_layout_t, Planar, NotStepX, Mutable>();
    test<gil::cmyk8c_planar_view_t, gil::cmyk_layout_t, Planar, NotStepX, Immutable>();
    test<gil::cmyk8_planar_step_view_t, gil::cmyk_layout_t, Planar, StepX, Mutable>();
    test<gil::cmyk8c_planar_step_view_t, gil::cmyk_layout_t, Planar, StepX, Immutable>();
}
