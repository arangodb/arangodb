//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/pixel.hpp>
#include <boost/gil/concepts/pixel.hpp>
#include <boost/gil/typedefs.hpp>

#include <boost/mp11.hpp>

namespace gil = boost::gil;
using namespace boost::mp11;

template <std::size_t NumChannels>
struct assert_num_channels
{
    template <typename Pixel>
    void operator()(Pixel&&)
    {
        static_assert(
            gil::num_channels<Pixel>::value == NumChannels,
            "pixels does not have expected number of channels");

        // TODO: Verify num_channels type with std::is_same
        // e.g. is std::integral_constant<T, ...>
    }
};

template <std::size_t NumChannels, typename... Pixels>
void test()
{
    mp_for_each<Pixels...>(assert_num_channels<NumChannels>());
}


int main()
{
    using one = mp_list
    <
        gil::gray8_pixel_t,
        gil::gray8c_pixel_t,
        gil::gray8s_pixel_t,
        gil::gray8sc_pixel_t,
        gil::gray16_pixel_t,
        gil::gray16c_pixel_t,
        gil::gray16s_pixel_t,
        gil::gray16sc_pixel_t,
        gil::gray16_pixel_t,
        gil::gray32_pixel_t,
        gil::gray32c_pixel_t,
        gil::gray32f_pixel_t,
        gil::gray32fc_pixel_t,
        gil::gray32s_pixel_t,
        gil::gray32sc_pixel_t
    >;
    test<1, one>();

    using three = mp_list
    <
        gil::bgr8_pixel_t,
        gil::bgr8c_pixel_t,
        gil::bgr8s_pixel_t,
        gil::bgr8sc_pixel_t,
        gil::bgr16_pixel_t,
        gil::bgr16c_pixel_t,
        gil::bgr16s_pixel_t,
        gil::bgr16sc_pixel_t,
        gil::bgr32_pixel_t,
        gil::bgr32c_pixel_t,
        gil::bgr32f_pixel_t,
        gil::bgr32fc_pixel_t,
        gil::bgr32s_pixel_t,
        gil::bgr32sc_pixel_t,
        gil::rgb8_pixel_t,
        gil::rgb8c_pixel_t,
        gil::rgb8s_pixel_t,
        gil::rgb8sc_pixel_t,
        gil::rgb16_pixel_t,
        gil::rgb16c_pixel_t,
        gil::rgb16s_pixel_t,
        gil::rgb16sc_pixel_t,
        gil::rgb32_pixel_t,
        gil::rgb32c_pixel_t,
        gil::rgb32f_pixel_t,
        gil::rgb32fc_pixel_t,
        gil::rgb32s_pixel_t,
        gil::rgb32sc_pixel_t
    >;
    test<3, three>();

    using four = mp_list
    <
        gil::abgr8_pixel_t,
        gil::abgr8c_pixel_t,
        gil::abgr8s_pixel_t,
        gil::abgr8sc_pixel_t,
        gil::abgr16_pixel_t,
        gil::abgr16c_pixel_t,
        gil::abgr16s_pixel_t,
        gil::abgr16sc_pixel_t,
        gil::abgr32_pixel_t,
        gil::abgr32c_pixel_t,
        gil::abgr32f_pixel_t,
        gil::abgr32fc_pixel_t,
        gil::abgr32s_pixel_t,
        gil::abgr32sc_pixel_t,
        gil::bgra8_pixel_t,
        gil::bgra8c_pixel_t,
        gil::bgra8s_pixel_t,
        gil::bgra8sc_pixel_t,
        gil::bgra16_pixel_t,
        gil::bgra16c_pixel_t,
        gil::bgra16s_pixel_t,
        gil::bgra16sc_pixel_t,
        gil::bgra32_pixel_t,
        gil::bgra32c_pixel_t,
        gil::bgra32f_pixel_t,
        gil::bgra32fc_pixel_t,
        gil::bgra32s_pixel_t,
        gil::bgra32sc_pixel_t,
        gil::cmyk8_pixel_t,
        gil::cmyk8c_pixel_t,
        gil::cmyk8s_pixel_t,
        gil::cmyk8sc_pixel_t,
        gil::cmyk16_pixel_t,
        gil::cmyk16c_pixel_t,
        gil::cmyk16s_pixel_t,
        gil::cmyk16sc_pixel_t,
        gil::cmyk32_pixel_t,
        gil::cmyk32c_pixel_t,
        gil::cmyk32f_pixel_t,
        gil::cmyk32fc_pixel_t,
        gil::cmyk32s_pixel_t,
        gil::cmyk32sc_pixel_t,
        gil::rgba8_pixel_t,
        gil::rgba8c_pixel_t,
        gil::rgba8s_pixel_t,
        gil::rgba8sc_pixel_t,
        gil::rgba16_pixel_t,
        gil::rgba16c_pixel_t,
        gil::rgba16s_pixel_t,
        gil::rgba16sc_pixel_t,
        gil::rgba32_pixel_t,
        gil::rgba32c_pixel_t,
        gil::rgba32f_pixel_t,
        gil::rgba32fc_pixel_t,
        gil::rgba32s_pixel_t,
        gil::rgba32sc_pixel_t
    >;
    test<4, four>();
}
