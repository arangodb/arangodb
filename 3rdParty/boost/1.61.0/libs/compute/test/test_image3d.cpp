//---------------------------------------------------------------------------//
// Copyright (c) 2013-2015 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestImage3D
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/image/image3d.hpp>

#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(image3d_get_supported_formats)
{
    const std::vector<compute::image_format> formats =
        compute::image3d::get_supported_formats(context);
}

// check type_name() for image3d
BOOST_AUTO_TEST_CASE(image3d_type_name)
{
    BOOST_CHECK(
        std::strcmp(
            boost::compute::type_name<boost::compute::image3d>(), "image3d_t"
        ) == 0
    );
}

BOOST_AUTO_TEST_SUITE_END()
