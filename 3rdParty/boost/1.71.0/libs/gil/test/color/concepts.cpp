//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_USE_CONCEPT_CHECK
#error Compile with BOOST_GIL_USE_CONCEPT_CHECK defined
#endif
#include <boost/gil/concepts.hpp>
#include <boost/gil/device_n.hpp>
#include <boost/gil/gray.hpp>
#include <boost/gil/rgb.hpp>
#include <boost/gil/rgba.hpp>
#include <boost/gil/cmyk.hpp>

#include <boost/concept_check.hpp>

namespace gil = boost::gil;
using boost::function_requires;

int main()
{
    // TODO: Most constraints() for color concepts are no-op as not implemented.
    //       Once implemented, this test should help to catch any issues early.

    function_requires<gil::ColorSpaceConcept<gil::devicen_t<2>>>();
    function_requires<gil::ColorSpaceConcept<gil::devicen_t<3>>>();
    function_requires<gil::ColorSpaceConcept<gil::devicen_t<4>>>();
    function_requires<gil::ColorSpaceConcept<gil::devicen_t<5>>>();
    function_requires<gil::ColorSpaceConcept<gil::gray_t>>();
    function_requires<gil::ColorSpaceConcept<gil::cmyk_t>>();
    function_requires<gil::ColorSpaceConcept<gil::rgb_t>>();
    function_requires<gil::ColorSpaceConcept<gil::rgba_t>>();

    function_requires<gil::ColorSpacesCompatibleConcept<gil::gray_t, gil::gray_t>>();
    function_requires<gil::ColorSpacesCompatibleConcept<gil::cmyk_t, gil::cmyk_t>>();
    function_requires<gil::ColorSpacesCompatibleConcept<gil::rgb_t, gil::rgb_t>>();
    function_requires<gil::ColorSpacesCompatibleConcept<gil::rgba_t, gil::rgba_t>>();
    function_requires<gil::ColorSpacesCompatibleConcept
        <
            gil::devicen_t<2>,
            gil::devicen_t<2>
        >>();
}
