//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>

namespace gil = boost::gil;

template <typename View>
void test_view_is_step_in_xy()
{
    static_assert(gil::view_is_step_in_x<View>::value, "view should support horizontal step");
    static_assert(gil::view_is_step_in_y<View>::value, "view should support vertical step");
}

template <typename View>
void test_view_is_step_in_y()
{
    static_assert(gil::view_is_step_in_y<View>::value, "view should support vertical step");
}

template <typename View>
void test_view_is_step_in_x_not()
{
    static_assert(!gil::view_is_step_in_x<View>::value, "view should not support horizontal step");
}

int main()
{
    test_view_is_step_in_xy<gil::gray8_step_view_t>();
    test_view_is_step_in_xy<gil::abgr8_step_view_t>();
    test_view_is_step_in_xy<gil::argb8_step_view_t>();
    test_view_is_step_in_xy<gil::bgr8_step_view_t>();
    test_view_is_step_in_xy<gil::bgra8_step_view_t>();
    test_view_is_step_in_xy<gil::rgb8_step_view_t>();
    test_view_is_step_in_xy<gil::rgba8_step_view_t>();
    test_view_is_step_in_xy<gil::rgba8_planar_step_view_t>();
    test_view_is_step_in_xy<gil::cmyk8_step_view_t>();
    test_view_is_step_in_xy<gil::cmyk8_planar_step_view_t>();

    test_view_is_step_in_y<gil::rgb8_view_t>();
    test_view_is_step_in_y<gil::rgb8_planar_view_t>();
    test_view_is_step_in_y<gil::rgba8_view_t>();
    test_view_is_step_in_y<gil::rgba8_planar_view_t>();
    test_view_is_step_in_y<gil::cmyk8_view_t>();
    test_view_is_step_in_y<gil::cmyk8_planar_view_t>();

    test_view_is_step_in_x_not<gil::rgb8_view_t>();
    test_view_is_step_in_x_not<gil::rgb8_planar_view_t>();
    test_view_is_step_in_x_not<gil::rgba8_view_t>();
    test_view_is_step_in_x_not<gil::rgba8_planar_view_t>();
    test_view_is_step_in_x_not<gil::cmyk8_view_t>();
    test_view_is_step_in_x_not<gil::cmyk8_planar_view_t>();
}
