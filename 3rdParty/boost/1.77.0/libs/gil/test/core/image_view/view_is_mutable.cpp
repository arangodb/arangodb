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
void test_view_is_mutable()
{
    static_assert(gil::view_is_mutable<View>::value, "view should be mutable");
}

template <typename View>
void test_view_is_mutable_not()
{
    static_assert(!gil::view_is_mutable<View>::value, "view should not be mutable");
}

int main()
{
    test_view_is_mutable<gil::gray8_view_t>();
    test_view_is_mutable_not<gil::gray8c_view_t>();

    test_view_is_mutable<gil::abgr8_view_t>();
    test_view_is_mutable<gil::abgr8_step_view_t>();
    test_view_is_mutable_not<gil::abgr8c_view_t>();
    test_view_is_mutable_not<gil::abgr8c_step_view_t>();

    test_view_is_mutable<gil::argb8_view_t>();
    test_view_is_mutable<gil::argb8_step_view_t>();
    test_view_is_mutable_not<gil::argb8c_view_t>();
    test_view_is_mutable_not<gil::argb8c_step_view_t>();

    test_view_is_mutable<gil::bgr8_view_t>();
    test_view_is_mutable<gil::bgr8_step_view_t>();
    test_view_is_mutable_not<gil::bgr8c_view_t>();
    test_view_is_mutable_not<gil::bgr8c_step_view_t>();

    test_view_is_mutable<gil::bgra8_view_t>();
    test_view_is_mutable<gil::bgra8_step_view_t>();
    test_view_is_mutable_not<gil::bgra8c_view_t>();
    test_view_is_mutable_not<gil::bgra8c_step_view_t>();

    test_view_is_mutable<gil::rgb8_view_t>();
    test_view_is_mutable<gil::rgb8_step_view_t>();
    test_view_is_mutable<gil::rgb8_planar_view_t>();
    test_view_is_mutable<gil::rgb8_planar_step_view_t>();
    test_view_is_mutable_not<gil::rgb8c_view_t>();
    test_view_is_mutable_not<gil::rgb8c_step_view_t>();
    test_view_is_mutable_not<gil::rgb8c_planar_view_t>();
    test_view_is_mutable_not<gil::rgb8c_planar_step_view_t>();

    test_view_is_mutable<gil::rgba8_view_t>();
    test_view_is_mutable<gil::rgba8_step_view_t>();
    test_view_is_mutable<gil::rgba8_planar_view_t>();
    test_view_is_mutable<gil::rgba8_planar_step_view_t>();
    test_view_is_mutable_not<gil::rgba8c_view_t>();
    test_view_is_mutable_not<gil::rgba8c_step_view_t>();
    test_view_is_mutable_not<gil::rgba8c_planar_view_t>();
    test_view_is_mutable_not<gil::rgba8c_planar_step_view_t>();

    test_view_is_mutable<gil::cmyk8_view_t>();
    test_view_is_mutable<gil::cmyk8_step_view_t>();
    test_view_is_mutable_not<gil::cmyk8c_view_t>();
    test_view_is_mutable_not<gil::cmyk8c_step_view_t>();

    test_view_is_mutable<gil::cmyk8_planar_view_t>();
    test_view_is_mutable<gil::cmyk8_planar_step_view_t>();
    test_view_is_mutable_not<gil::cmyk8c_planar_view_t>();
    test_view_is_mutable_not<gil::cmyk8c_planar_step_view_t>();
}
