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
#include <boost/gil/color_base.hpp>
#include <boost/gil/typedefs.hpp>

#include <boost/concept_check.hpp>

namespace gil = boost::gil;
using boost::function_requires;

int main()
{
    function_requires<gil::ColorBaseConcept
        <
            gil::detail::homogeneous_color_base<std::uint8_t, gil::gray_layout_t, 1>
        >>();

    function_requires<gil::HomogeneousColorBaseConcept
        <
            gil::detail::homogeneous_color_base<std::uint8_t, gil::gray_layout_t, 1>
        >>();

    function_requires<gil::HomogeneousColorBaseValueConcept
        <
            gil::detail::homogeneous_color_base<std::uint8_t, gil::gray_layout_t, 1>
        >>();

    // FIXME: https://github.com/boostorg/gil/issues/271
    //function_requires
    //<
    //    gil::ColorBaseConcept
    //    <
    //        gil::detail::homogeneous_color_base<std::uint8_t, gil::rgb_layout_t, 3>
    //    >
    //>();

    function_requires<gil::ColorBaseConcept<gil::gray8_pixel_t>>();
    function_requires<gil::ColorBaseConcept<gil::rgb8_pixel_t>>();
    function_requires<gil::ColorBaseConcept<gil::bgr8_pixel_t>>();

}
