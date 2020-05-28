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
#include <boost/gil.hpp>

#include <boost/concept_check.hpp>

#include <array>

namespace gil = boost::gil;
using boost::function_requires;

int main()
{
    function_requires<gil::ImageConcept<gil::gray8_image_t>>();
    function_requires<gil::ImageConcept<gil::rgb8_image_t>>();

    return 0;
}
