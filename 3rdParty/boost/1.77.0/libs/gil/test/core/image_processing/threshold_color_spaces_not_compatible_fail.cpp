//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/image_processing/threshold.hpp>

namespace gil = boost::gil;

int main()
{
    // Source and destination views must have pixels with the same (compatible) color space
    {
        gil::rgb8_image_t src;
        gil::gray8_image_t dst;
        gil::threshold_binary(const_view(src), view(dst), 0, 255);
    }
    {
        gil::gray8_image_t src;
        gil::rgb8_image_t dst;
        gil::threshold_binary(const_view(src), view(dst), 0, 255);
    }
}
