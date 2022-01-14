//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/color_base_algorithm.hpp>
#include <boost/gil/pixel.hpp>
#include <boost/gil/typedefs.hpp>

#include <cassert>
#include <cstdint>

namespace gil = boost::gil;

int main()
{
    // K-element index must be less than size of channel_mapping_t sequence
    // of the *destination* color base.

    // RGB (wider space) transformation to Gray (narrower space) takes only R channel value
    {
        gil::cmyk8_pixel_t src{16, 32, 64, 128};
        gil::rgb8_pixel_t  dst{0, 0, 0};
        gil::static_transform(src, dst, [](std::uint8_t src_channel) {
            return src_channel; // copy
            });
        assert(gil::at_c<0>(dst) == std::uint16_t{16});
        assert(gil::at_c<0>(dst) == std::uint16_t{32});
        assert(gil::at_c<0>(dst) == std::uint16_t{64});
    }

    // RGB (narrower space) to CMYK (wider space) transformation FAILS to compile
    {
        gil::rgb8_pixel_t  src{16, 32, 64};
        gil::cmyk8_pixel_t dst{0, 0, 0, 0};
        gil::static_transform(src, dst, [](std::uint8_t src_channel) {
            return src_channel; // copy
        });
    }
}
