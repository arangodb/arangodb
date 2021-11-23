//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/io/tiff.hpp>
#include <boost/gil/io/typedefs.hpp>

#include <boost/core/lightweight_test.hpp>

#include "color_space_write_test.hpp"
#include "mandel_view.hpp"
#include "paths.hpp"

namespace gil = boost::gil;

void test_rgb_color_space_write()
{
    color_space_write_test<gil::tiff_tag>(
        tiff_out + "rgb_color_space_test.tif", tiff_out + "bgr_color_space_test.tif");
}

int main()
{
    test_rgb_color_space_write();

    return boost::report_errors();
}
