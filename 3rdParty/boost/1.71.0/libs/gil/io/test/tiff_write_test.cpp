//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/io/typedefs.hpp>
#include <boost/gil/extension/io/tiff.hpp>

#include <boost/test/unit_test.hpp>

#include "color_space_write_test.hpp"
#include "mandel_view.hpp"
#include "paths.hpp"

using namespace std;
using namespace boost::gil;

using tag_t = tiff_tag;

BOOST_AUTO_TEST_SUITE( gil_io_tiff_tests )

BOOST_AUTO_TEST_CASE( rgb_color_space_write_test )
{
    color_space_write_test< tag_t >( tiff_out + "rgb_color_space_test.tif"
                                   , tiff_out + "bgr_color_space_test.tif"
                                   );
}

BOOST_AUTO_TEST_SUITE_END()
