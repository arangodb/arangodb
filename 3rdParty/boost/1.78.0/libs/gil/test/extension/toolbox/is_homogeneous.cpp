//
// Copyright 2013 Christian Henning
// Copyright 2020 Mateusz Loskot <mateusz@loskot.net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/metafunctions/is_homogeneous.hpp>

namespace gil = boost::gil;

void test_is_homogeneous()
{
    static_assert(gil::is_homogeneous<gil::rgb8_pixel_t>::value, "");
    static_assert(gil::is_homogeneous<gil::cmyk16c_planar_ref_t>::value, "");

    using image_t = gil::bit_aligned_image1_type< 4, gil::gray_layout_t>::type;
    static_assert(gil::is_homogeneous<image_t::view_t::reference>::value, "");
}

int main()
{
    test_is_homogeneous();
}
