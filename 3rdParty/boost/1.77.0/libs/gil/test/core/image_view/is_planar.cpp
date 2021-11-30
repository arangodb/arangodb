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
void test_is_planar()
{
    static_assert(gil::is_planar<View>::value, "view should be planar");
}

template <typename View>
void test_is_planar_not()
{
    static_assert(!gil::is_planar<View>::value, "view should not be planar");
}


int main()
{
    test_is_planar_not<gil::gray8_view_t>();
    test_is_planar_not<gil::gray8c_view_t>();

    test_is_planar_not<gil::abgr8_view_t>();
    test_is_planar_not<gil::abgr8_step_view_t>();
    test_is_planar_not<gil::abgr8c_view_t>();
    test_is_planar_not<gil::abgr8c_step_view_t>();

    test_is_planar_not<gil::argb8_view_t>();
    test_is_planar_not<gil::argb8_step_view_t>();
    test_is_planar_not<gil::argb8c_view_t>();
    test_is_planar_not<gil::argb8c_step_view_t>();

    test_is_planar_not<gil::bgr8_view_t>();
    test_is_planar_not<gil::bgr8_step_view_t>();
    test_is_planar_not<gil::bgr8c_view_t>();
    test_is_planar_not<gil::bgr8c_step_view_t>();

    test_is_planar_not<gil::bgra8_view_t>();
    test_is_planar_not<gil::bgra8_step_view_t>();
    test_is_planar_not<gil::bgra8c_view_t>();
    test_is_planar_not<gil::bgra8c_step_view_t>();

    test_is_planar_not<gil::rgb8_view_t>();
    test_is_planar_not<gil::rgb8_step_view_t>();
    test_is_planar_not<gil::rgb8c_view_t>();
    test_is_planar_not<gil::rgb8c_step_view_t>();
    test_is_planar<gil::rgb8_planar_view_t>();
    test_is_planar<gil::rgb8_planar_step_view_t>();
    test_is_planar<gil::rgb8c_planar_view_t>();
    test_is_planar<gil::rgb8c_planar_step_view_t>();

    test_is_planar_not<gil::rgba8_view_t>();
    test_is_planar_not<gil::rgba8_step_view_t>();
    test_is_planar_not<gil::rgba8c_view_t>();
    test_is_planar_not<gil::rgba8c_step_view_t>();
    test_is_planar<gil::rgba8_planar_view_t>();
    test_is_planar<gil::rgba8_planar_step_view_t>();
    test_is_planar<gil::rgba8c_planar_view_t>();
    test_is_planar<gil::rgba8c_planar_step_view_t>();

    test_is_planar_not<gil::cmyk8_view_t>();
    test_is_planar_not<gil::cmyk8_step_view_t>();
    test_is_planar_not<gil::cmyk8c_view_t>();
    test_is_planar_not<gil::cmyk8c_step_view_t>();

    test_is_planar<gil::cmyk8_planar_view_t>();
    test_is_planar<gil::cmyk8_planar_step_view_t>();
    test_is_planar<gil::cmyk8c_planar_view_t>();
    test_is_planar<gil::cmyk8c_planar_step_view_t>();
}
