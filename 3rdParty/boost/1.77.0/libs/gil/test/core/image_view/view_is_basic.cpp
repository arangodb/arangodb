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
void test_view_is_basic()
{
    static_assert(gil::view_is_basic<View>::value, "view should be basic");
}

template <typename View>
void test_view_is_basic_not()
{
    static_assert(!gil::view_is_basic<View>::value, "view should not be basic");
}

int main()
{
    test_view_is_basic<gil::gray8_view_t>();
    test_view_is_basic<gil::gray8c_view_t>();

    test_view_is_basic<gil::abgr8_view_t>();
    test_view_is_basic<gil::abgr8_step_view_t>();
    test_view_is_basic<gil::abgr8c_view_t>();
    test_view_is_basic<gil::abgr8c_step_view_t>();

    test_view_is_basic<gil::argb8_view_t>();
    test_view_is_basic<gil::argb8_step_view_t>();
    test_view_is_basic<gil::argb8c_view_t>();
    test_view_is_basic<gil::argb8c_step_view_t>();

    test_view_is_basic<gil::bgr8_view_t>();
    test_view_is_basic<gil::bgr8_step_view_t>();
    test_view_is_basic<gil::bgr8c_view_t>();
    test_view_is_basic<gil::bgr8c_step_view_t>();

    test_view_is_basic<gil::bgra8_view_t>();
    test_view_is_basic<gil::bgra8_step_view_t>();
    test_view_is_basic<gil::bgra8c_view_t>();
    test_view_is_basic<gil::bgra8c_step_view_t>();

    test_view_is_basic<gil::rgb8_view_t>();
    test_view_is_basic<gil::rgb8_step_view_t>();
    test_view_is_basic<gil::rgb8_planar_view_t>();
    test_view_is_basic<gil::rgb8_planar_step_view_t>();
    test_view_is_basic<gil::rgb8c_view_t>();
    test_view_is_basic<gil::rgb8c_step_view_t>();
    test_view_is_basic<gil::rgb8c_planar_view_t>();
    test_view_is_basic<gil::rgb8c_planar_step_view_t>();

    test_view_is_basic<gil::rgba8_view_t>();
    test_view_is_basic<gil::rgba8_step_view_t>();
    test_view_is_basic<gil::rgba8_planar_view_t>();
    test_view_is_basic<gil::rgba8_planar_step_view_t>();
    test_view_is_basic<gil::rgba8c_view_t>();
    test_view_is_basic<gil::rgba8c_step_view_t>();
    test_view_is_basic<gil::rgba8c_planar_view_t>();
    test_view_is_basic<gil::rgba8c_planar_step_view_t>();

    test_view_is_basic<gil::cmyk8_view_t>();
    test_view_is_basic<gil::cmyk8_step_view_t>();
    test_view_is_basic<gil::cmyk8c_view_t>();
    test_view_is_basic<gil::cmyk8c_step_view_t>();

    test_view_is_basic<gil::cmyk8_planar_view_t>();
    test_view_is_basic<gil::cmyk8_planar_step_view_t>();
    test_view_is_basic<gil::cmyk8c_planar_view_t>();
    test_view_is_basic<gil::cmyk8c_planar_step_view_t>();
}
