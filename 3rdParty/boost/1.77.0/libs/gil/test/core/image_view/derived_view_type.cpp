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

int main()
{

    static_assert(std::is_same
        <
            gil::derived_view_type<gil::cmyk8c_planar_step_view_t>::type,
            gil::cmyk8c_planar_step_view_t
        >::value, "derived_view_type should be cmyk8c_planar_step_view_t");

    static_assert(std::is_same
        <
            gil::derived_view_type
            <
                gil::cmyk8c_planar_step_view_t, std::uint16_t, gil::rgb_layout_t
            >::type,
            gil::rgb16c_planar_step_view_t
        >::value, "derived_view_type should be rgb16c_planar_step_view_t");

    static_assert(std::is_same
        <
            gil::derived_view_type
            <
                gil::cmyk8c_planar_step_view_t,
                boost::use_default,
                gil::rgb_layout_t,
                std::false_type,
                boost::use_default,
                std::false_type
            >::type,
            gil::rgb8c_step_view_t
        >::value, "derived_view_type should be rgb8c_step_view_t");
}
